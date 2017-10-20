// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/synchronous_shutdown_object_container_impl.h"

#include "base/time/default_clock.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/active_host_network_state_updater.h"
#include "chromeos/components/tether/asynchronous_shutdown_object_container.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/host_connection_metrics_logger.h"
#include "chromeos/components/tether/host_scan_device_prioritizer_impl.h"
#include "chromeos/components/tether/host_scan_scheduler_impl.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/components/tether/keep_alive_scheduler.h"
#include "chromeos/components/tether/master_host_scan_cache.h"
#include "chromeos/components/tether/network_connection_handler_tether_delegate.h"
#include "chromeos/components/tether/network_host_scan_cache.h"
#include "chromeos/components/tether/network_list_sorter.h"
#include "chromeos/components/tether/notification_remover.h"
#include "chromeos/components/tether/persistent_host_scan_cache_impl.h"
#include "chromeos/components/tether/tether_connector_impl.h"
#include "chromeos/components/tether/tether_disconnector_impl.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"
#include "chromeos/components/tether/tether_network_disconnection_handler.h"
#include "chromeos/components/tether/timer_factory.h"
#include "chromeos/components/tether/wifi_hotspot_connector.h"

namespace chromeos {

namespace tether {

// static
SynchronousShutdownObjectContainerImpl::Factory*
    SynchronousShutdownObjectContainerImpl::Factory::factory_instance_ =
        nullptr;

// static
std::unique_ptr<SynchronousShutdownObjectContainer>
SynchronousShutdownObjectContainerImpl::Factory::NewInstance(
    AsynchronousShutdownObjectContainer* asychronous_container,
    NotificationPresenter* notification_presenter,
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    NetworkConnect* network_connect,
    NetworkConnectionHandler* network_connection_handler) {
  if (!factory_instance_)
    factory_instance_ = new Factory();

  return factory_instance_->BuildInstance(
      asychronous_container, notification_presenter, pref_service,
      network_state_handler, network_connect, network_connection_handler);
}

// static
void SynchronousShutdownObjectContainerImpl::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

SynchronousShutdownObjectContainerImpl::Factory::~Factory() {}

std::unique_ptr<SynchronousShutdownObjectContainer>
SynchronousShutdownObjectContainerImpl::Factory::BuildInstance(
    AsynchronousShutdownObjectContainer* asychronous_container,
    NotificationPresenter* notification_presenter,
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    NetworkConnect* network_connect,
    NetworkConnectionHandler* network_connection_handler) {
  return base::MakeUnique<SynchronousShutdownObjectContainerImpl>(
      asychronous_container, notification_presenter, pref_service,
      network_state_handler, network_connect, network_connection_handler);
}

SynchronousShutdownObjectContainerImpl::SynchronousShutdownObjectContainerImpl(
    AsynchronousShutdownObjectContainer* asychronous_container,
    NotificationPresenter* notification_presenter,
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    NetworkConnect* network_connect,
    NetworkConnectionHandler* network_connection_handler)
    : network_state_handler_(network_state_handler),
      network_list_sorter_(base::MakeUnique<NetworkListSorter>()),
      tether_host_response_recorder_(
          base::MakeUnique<TetherHostResponseRecorder>(pref_service)),
      device_id_tether_network_guid_map_(
          base::MakeUnique<DeviceIdTetherNetworkGuidMap>()),
      host_scan_device_prioritizer_(
          base::MakeUnique<HostScanDevicePrioritizerImpl>(
              tether_host_response_recorder_.get())),
      wifi_hotspot_connector_(
          base::MakeUnique<WifiHotspotConnector>(network_state_handler_,
                                                 network_connect)),
      active_host_(base::MakeUnique<ActiveHost>(
          asychronous_container->tether_host_fetcher(),
          pref_service)),
      active_host_network_state_updater_(
          base::MakeUnique<ActiveHostNetworkStateUpdater>(
              active_host_.get(),
              network_state_handler_)),
      persistent_host_scan_cache_(
          base::MakeUnique<PersistentHostScanCacheImpl>(pref_service)),
      network_host_scan_cache_(base::MakeUnique<NetworkHostScanCache>(
          network_state_handler_,
          tether_host_response_recorder_.get(),
          device_id_tether_network_guid_map_.get())),
      master_host_scan_cache_(base::MakeUnique<MasterHostScanCache>(
          base::MakeUnique<TimerFactory>(),
          active_host_.get(),
          network_host_scan_cache_.get(),
          persistent_host_scan_cache_.get())),
      notification_remover_(
          base::MakeUnique<NotificationRemover>(network_state_handler_,
                                                notification_presenter,
                                                master_host_scan_cache_.get(),
                                                active_host_.get())),
      keep_alive_scheduler_(base::MakeUnique<KeepAliveScheduler>(
          active_host_.get(),
          asychronous_container->ble_connection_manager(),
          master_host_scan_cache_.get(),
          device_id_tether_network_guid_map_.get())),
      clock_(base::MakeUnique<base::DefaultClock>()),
      host_scanner_(base::MakeUnique<HostScanner>(
          network_state_handler_,
          asychronous_container->tether_host_fetcher(),
          asychronous_container->ble_connection_manager(),
          host_scan_device_prioritizer_.get(),
          tether_host_response_recorder_.get(),
          notification_presenter,
          device_id_tether_network_guid_map_.get(),
          master_host_scan_cache_.get(),
          clock_.get())),
      host_scan_scheduler_(
          base::MakeUnique<HostScanSchedulerImpl>(network_state_handler_,
                                                  host_scanner_.get())),
      host_connection_metrics_logger_(
          base::MakeUnique<HostConnectionMetricsLogger>()),
      tether_connector_(base::MakeUnique<TetherConnectorImpl>(
          network_state_handler_,
          wifi_hotspot_connector_.get(),
          active_host_.get(),
          asychronous_container->tether_host_fetcher(),
          asychronous_container->ble_connection_manager(),
          tether_host_response_recorder_.get(),
          device_id_tether_network_guid_map_.get(),
          master_host_scan_cache_.get(),
          notification_presenter,
          host_connection_metrics_logger_.get(),
          asychronous_container->disconnect_tethering_request_sender(),
          asychronous_container->wifi_hotspot_disconnector())),
      tether_disconnector_(base::MakeUnique<TetherDisconnectorImpl>(
          active_host_.get(),
          asychronous_container->wifi_hotspot_disconnector(),
          asychronous_container->disconnect_tethering_request_sender(),
          tether_connector_.get(),
          device_id_tether_network_guid_map_.get())),
      tether_network_disconnection_handler_(
          base::MakeUnique<TetherNetworkDisconnectionHandler>(
              active_host_.get(),
              network_state_handler_,
              asychronous_container->network_configuration_remover(),
              asychronous_container->disconnect_tethering_request_sender())),
      network_connection_handler_tether_delegate_(
          base::MakeUnique<NetworkConnectionHandlerTetherDelegate>(
              network_connection_handler,
              active_host_.get(),
              tether_connector_.get(),
              tether_disconnector_.get())) {
  network_state_handler_->set_tether_sort_delegate(network_list_sorter_.get());
}

SynchronousShutdownObjectContainerImpl::
    ~SynchronousShutdownObjectContainerImpl() {
  network_state_handler_->set_tether_sort_delegate(nullptr);
}

ActiveHost* SynchronousShutdownObjectContainerImpl::active_host() {
  return active_host_.get();
}

HostScanCache* SynchronousShutdownObjectContainerImpl::host_scan_cache() {
  return master_host_scan_cache_.get();
}

HostScanScheduler*
SynchronousShutdownObjectContainerImpl::host_scan_scheduler() {
  return host_scan_scheduler_.get();
}

TetherDisconnector*
SynchronousShutdownObjectContainerImpl::tether_disconnector() {
  return tether_disconnector_.get();
}

}  // namespace tether

}  // namespace chromeos
