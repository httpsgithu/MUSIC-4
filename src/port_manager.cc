#include "music/port_manager.hh"

#if MUSIC_USE_MPI
#include <mpi.h>

#include "music/parse.hh"
#include "music/application_mapper.hh"
#include <strings.h>
#include <fstream>

namespace MUSIC
{
	static std::string err_no_app_info = "No ApplicationInfo object available";

	const ConnectivityInfo& PortConnectivityManager::portConnectivity (const std::string identifier) const
	{
		return *config_.connectivityMap ()->info (identifier);
	}

	bool PortConnectivityManager::isInstantiated (std::string identifier)
	{
		auto map_iterator = portMap_.find (identifier);
		if (map_iterator == portMap_.end() || map_iterator->second.expired ())
			return false;
		return true;
	}

	SPVec<Port> PortConnectivityManager::getPorts ()
	{
		SPVec<Port> v;
		for (auto& p : portMap_)
		{
			auto spt = p.second.lock ();
			if (spt)
				v.push_back (spt);
		}
		return v;
	}

	void PortConnectivityManager::updatePorts ()
	{
		// only perform expensive updates when necessary
		if (modified_)
		{
			for (auto& port : getPorts ())
				port->reconnect ();
			modified_ = false;
		}
	}

	void PortConnectivityManager::removePort (std::string identifier)
	{
		if (!isInstantiated (identifier))
			error (std::string ("Can not remove port. \
						There is no instance of Port with the given port name."));
		if (isConnected (identifier))
			error (std::string ("Can not remove port. \
						Ports must be disconnected before they can be removed."));
		portMap_.erase (identifier);
	}

	void PortConnectivityManager::connect (std::string senderApp, std::string senderPort,
			std::string receiverApp, std::string receiverPort,
			int width,
			ConnectorInfo::CommunicationType commType,
			ConnectorInfo::ProcessingMethod procMethod)
	{
		// To keep the maxPortCode synchron over all MPI processes,
		// this must be executed on all processes (even if they do not handle the
		// requested connection)
		int portCode = ConnectorInfo::allocPortCode();

		ConnectivityInfo::PortDirection dir;
		const ApplicationInfo* remoteInfo;
		if (config_.Name ()== senderApp)
		{
			// if this app is sender
			dir = ConnectivityInfo::PortDirection::OUTPUT;
			remoteInfo = config_.applications ()->lookup (receiverApp);
		}
		else if (config_.Name() == receiverApp)
		{
			// if this app is receiver
			dir = ConnectivityInfo::PortDirection::INPUT;
			remoteInfo = config_.applications ()->lookup (senderApp);
		}
		else
		{
			// This connection is not handled by this app. Gracefully return
			return;
		}

		if (remoteInfo == nullptr)
			errorRank(err_no_app_info);

		// where to get the portCode from?
		int leader = remoteInfo->leader ();

		// TODO does it actually prevent creating the same connection twice?
		config_.connectivityMap()->add (
			dir == ConnectivityInfo::PortDirection::OUTPUT ? senderPort : receiverPort,
			dir, width, receiverApp, receiverPort, portCode, leader,
			remoteInfo->nProc (), commType, procMethod);
		modified_ = true;

	}

	void PortConnectivityManager::disconnect (std::string appName, std::string portName)
	{
		if (config_.Name ()== appName)
			config_.connectivityMap ()->remove (portName);
		else
		{
			auto connectedPorts = config_.connectivityMap ()->getConnectedLocalPorts (portName, appName);
			for (auto& localPort : connectedPorts)
				config_. connectivityMap ()->remove (localPort, appName, portName);
		}
		modified_ = true;
	}

	void PortConnectivityManager::disconnect (std::string senderApp, std::string senderPort, std::string receiverApp, std::string receiverPort)
	{
		if (config_. Name ()== senderApp)
			config_. connectivityMap ()->remove (senderPort, receiverApp, receiverPort);
		else if (config_. Name ()== receiverApp)
			config_. connectivityMap ()->remove (receiverPort, senderApp, senderPort);
		else
			return;
		modified_ = true;
	}

	bool PortConnectivityManager::isConnected (std::string identifier) const
	{
		return config_. connectivityMap ()->isConnected (identifier);
	}

	void PortConnectivityManager::finalize ()
	{
		for (auto& p : getPorts ())
		{
			// TODO force removal of ports
		}
	}
}
#endif
