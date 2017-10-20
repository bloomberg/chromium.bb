// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_IMPL_H_

#include <memory>

#include "chromeos/components/tether/synchronous_shutdown_object_container.h"

class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace chromeos {

class NetworkStateHandler;
class NetworkConnect;
class NetworkConnectionHandler;

namespace tether {

class ActiveHost;
class ActiveHostNetworkStateUpdater;
class AsynchronousShutdownObjectContainer;
class NetworkConnectionHandlerTetherDelegate;
class DeviceIdTetherNetworkGuidMap;
class HostScanner;
class HostScanScheduler;
class HostScanDevicePrioritizerImpl;
class KeepAliveScheduler;
class HostConnectionMetricsLogger;
class MasterHostScanCache;
class NetworkHostScanCache;
class NetworkListSorter;
class NotificationPresenter;
class NotificationRemover;
class PersistentHostScanCache;
class TetherConnector;
class TetherDisconnector;
class TetherHostResponseRecorder;
class TetherNetworkDisconnectionHandler;
class WifiHotspotConnector;

// Concrete SynchronousShutdownObjectContainer implementation.
class SynchronousShutdownObjectContainerImpl
    : public SynchronousShutdownObjectContainer {
 public:
  class Factory {
   public:
    static std::unique_ptr<SynchronousShutdownObjectContainer> NewInstance(
        AsynchronousShutdownObjectContainer* asychronous_container,
        NotificationPresenter* notification_presenter,
        PrefService* pref_service,
        NetworkStateHandler* network_state_handler,
        NetworkConnect* network_connect,
        NetworkConnectionHandler* network_connection_handler);
    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<SynchronousShutdownObjectContainer> BuildInstance(
        AsynchronousShutdownObjectContainer* asychronous_container,
        NotificationPresenter* notification_presenter,
        PrefService* pref_service,
        NetworkStateHandler* network_state_handler,
        NetworkConnect* network_connect,
        NetworkConnectionHandler* network_connection_handler);
    virtual ~Factory();

   private:
    static Factory* factory_instance_;
  };

  SynchronousShutdownObjectContainerImpl(
      AsynchronousShutdownObjectContainer* asychronous_container,
      NotificationPresenter* notification_presenter,
      PrefService* pref_service,
      NetworkStateHandler* network_state_handler,
      NetworkConnect* network_connect,
      NetworkConnectionHandler* network_connection_handler);
  ~SynchronousShutdownObjectContainerImpl() override;

  // SynchronousShutdownObjectContainer:
  ActiveHost* active_host() override;
  HostScanCache* host_scan_cache() override;
  HostScanScheduler* host_scan_scheduler() override;
  TetherDisconnector* tether_disconnector() override;

 private:
  NetworkStateHandler* network_state_handler_;

  std::unique_ptr<NetworkListSorter> network_list_sorter_;
  std::unique_ptr<TetherHostResponseRecorder> tether_host_response_recorder_;
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  std::unique_ptr<HostScanDevicePrioritizerImpl> host_scan_device_prioritizer_;
  std::unique_ptr<WifiHotspotConnector> wifi_hotspot_connector_;
  std::unique_ptr<ActiveHost> active_host_;
  std::unique_ptr<ActiveHostNetworkStateUpdater>
      active_host_network_state_updater_;
  std::unique_ptr<PersistentHostScanCache> persistent_host_scan_cache_;
  std::unique_ptr<NetworkHostScanCache> network_host_scan_cache_;
  std::unique_ptr<MasterHostScanCache> master_host_scan_cache_;
  std::unique_ptr<NotificationRemover> notification_remover_;
  std::unique_ptr<KeepAliveScheduler> keep_alive_scheduler_;
  std::unique_ptr<base::Clock> clock_;
  std::unique_ptr<HostScanner> host_scanner_;
  std::unique_ptr<HostScanScheduler> host_scan_scheduler_;
  std::unique_ptr<HostConnectionMetricsLogger> host_connection_metrics_logger_;
  std::unique_ptr<TetherConnector> tether_connector_;
  std::unique_ptr<TetherDisconnector> tether_disconnector_;
  std::unique_ptr<TetherNetworkDisconnectionHandler>
      tether_network_disconnection_handler_;
  std::unique_ptr<NetworkConnectionHandlerTetherDelegate>
      network_connection_handler_tether_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousShutdownObjectContainerImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_IMPL_H_
