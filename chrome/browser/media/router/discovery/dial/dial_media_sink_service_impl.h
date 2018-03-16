// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/discovery/dial/device_description_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_registry.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/common/media_router/discovery/media_sink_service_base.h"
#include "chrome/common/media_router/discovery/media_sink_service_util.h"

namespace service_manager {
class Connector;
}

namespace media_router {

class DeviceDescriptionService;
class DialRegistry;

// A service which can be used to start background discovery and resolution of
// DIAL devices (Smart TVs, Game Consoles, etc.).
// This class may be created on any thread. All methods, unless otherwise noted,
// must be invoked on the SequencedTaskRunner given by |task_runner()|.
class DialMediaSinkServiceImpl : public MediaSinkServiceBase,
                                 public DialRegistry::Observer {
 public:
  // |connector|: connector to the ServiceManager suitable to use on
  // |task_runner|.
  // |on_sinks_discovered_cb|: Callback for MediaSinkServiceBase.
  // |dial_sink_added_cb|: If not null, callback to invoke when a DIAL sink has
  // been discovered.
  // Note that both callbacks are invoked on |task_runner|.
  // |task_runner|: The SequencedTaskRunner this class runs in.
  DialMediaSinkServiceImpl(
      std::unique_ptr<service_manager::Connector> connector,
      const OnSinksDiscoveredCallback& on_sinks_discovered_cb,
      const OnDialSinkAddedCallback& dial_sink_added_cb,
      const OnAvailableSinksUpdatedCallback& available_sinks_updated_callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~DialMediaSinkServiceImpl() override;

  virtual void Start();

  void OnUserGesture();

  // Returns the SequencedTaskRunner that should be used to invoke methods on
  // this instance. Can be invoked on any thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  // Starts monitoring available sinks for |app_name|. If available sinks
  // change, invokes |available_sinks_updated_callback_|.
  // Marked virtual for tests.
  virtual void StartMonitoringAvailableSinksForApp(const std::string& app_name);

  // Stops monitoring available sinks for |app_name|.
  // Marked virtual for tests.
  virtual void StopMonitoringAvailableSinksForApp(const std::string& app_name);

 protected:
  // Does not take ownership of |dial_registry|.
  void SetDialRegistryForTest(DialRegistry* dial_registry);
  void SetDescriptionServiceForTest(
      std::unique_ptr<DeviceDescriptionService> description_service);
  void SetAppDiscoveryServiceForTest(
      std::unique_ptr<DialAppDiscoveryService> app_discovery_service);

  // MediaSinkServiceBase implementation.
  void OnDiscoveryComplete() override;

 private:
  friend class DialMediaSinkServiceImplTest;
  friend class MockDialMediaSinkServiceImpl;
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest, TestStart);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDeviceDescriptionRestartsTimer);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDialDeviceEventRestartsTimer);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDeviceDescriptionAvailable);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDeviceDescriptionAvailableIPAddressChanged);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest, TestRestartAfterStop);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDialSinkAddedCallback);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           DialMediaSinkServiceObserver);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestStartStopMonitoringAvailableSinksForApp);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDialAppInfoAvailableNoStartMonitoring);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDialAppInfoAvailableStopsMonitoring);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDialAppInfoAvailableNoSink);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDialAppInfoAvailableSinksAdded);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDialAppInfoAvailableSinksRemoved);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDialAppInfoAvailableWithAlreadyAvailableSinks);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestFetchDialAppInfoWithCachedAppInfo);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestStartAfterStopMonitoringForApp);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestFetchDialAppInfoWithDiscoveryOnlySink);

  // DialRegistry::Observer implementation
  void OnDialDeviceEvent(const DialRegistry::DeviceList& devices) override;
  void OnDialError(DialRegistry::DialErrorCode type) override;

  // Called when description service successfully fetches and parses device
  // description XML. Restart |finish_timer_| if it is not running.
  void OnDeviceDescriptionAvailable(
      const DialDeviceData& device_data,
      const ParsedDialDeviceDescription& description_data);

  // Called when fails to fetch or parse device description XML.
  void OnDeviceDescriptionError(const DialDeviceData& device,
                                const std::string& error_message);

  // Called when app discovery service finishes fetching and parsing app info
  // XML.
  void OnAppInfoParseCompleted(const std::string& sink_id,
                               const std::string& app_name,
                               SinkAppStatus app_status);

  // Invokes |available_sinks_updated_callback_| with |app_name| and current
  // available sinks for |app_name|.
  void NotifySinkObservers(const std::string& app_name);

  // Queries app status of |app_name| on |dial_sink|.
  void FetchAppInfoForSink(const MediaSinkInternal& dial_sink,
                           const std::string& app_name);

  // Issues HTTP request to get status of all registered apps on current sinks.
  void RescanAppInfo();

  // Helper function to get app status from |app_statuses_|.
  SinkAppStatus GetAppStatus(const std::string& sink_id,
                             const std::string& app_name) const;

  // Helper function to set app status in |app_statuses_|.
  void SetAppStatus(const std::string& sink_id,
                    const std::string& app_name,
                    SinkAppStatus app_status);

  // MediaSinkServiceBase implementation.
  void RecordDeviceCounts() override;

  base::flat_set<MediaSinkInternal> GetAvailableSinks(
      const std::string& app_name) const;

  // Connector to ServiceManager for safe XML parsing requests.
  std::unique_ptr<service_manager::Connector> connector_;

  // Initialized in |Start()|.
  std::unique_ptr<DeviceDescriptionService> description_service_;

  // Initialized in |Start()|.
  std::unique_ptr<DialAppDiscoveryService> app_discovery_service_;

  OnDialSinkAddedCallback dial_sink_added_cb_;

  OnAvailableSinksUpdatedCallback available_sinks_updated_callback_;

  // Raw pointer to DialRegistry singleton.
  DialRegistry* dial_registry_ = nullptr;

  // DialRegistry for unit test.
  DialRegistry* test_dial_registry_ = nullptr;

  // Device data list from current round of discovery.
  DialRegistry::DeviceList current_devices_;

  // Map of app status, keyed by <sink id:app name>.
  base::flat_map<std::string, SinkAppStatus> app_statuses_;

  // Set of registered app names.
  base::flat_set<std::string> registered_apps_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DialDeviceCountMetrics metrics_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_IMPL_H_
