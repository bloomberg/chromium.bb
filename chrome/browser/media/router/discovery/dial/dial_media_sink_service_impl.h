// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/discovery/dial/device_description_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_registry.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/common/media_router/discovery/media_sink_service_base.h"

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
  // |request_context|: Used for network requests.
  // |task_runner|: The SequencedTaskRunner this class runs in.
  DialMediaSinkServiceImpl(
      std::unique_ptr<service_manager::Connector> connector,
      const OnSinksDiscoveredCallback& on_sinks_discovered_cb,
      const OnDialSinkAddedCallback& dial_sink_added_cb,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~DialMediaSinkServiceImpl() override;

  // Marked virtual for tests.
  virtual void Start();

  void OnUserGesture();

  // Returns the SequencedTaskRunner that should be used to invoke methods on
  // this instance. Can be invoked on any thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

 protected:
  // Does not take ownership of |dial_registry|.
  void SetDialRegistryForTest(DialRegistry* dial_registry);
  void SetDescriptionServiceForTest(
      std::unique_ptr<DeviceDescriptionService> description_service);

 private:
  friend class DialMediaSinkServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest, TestStart);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest, TestTimer);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           TestOnDeviceDescriptionAvailable);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest, TestRestartAfterStop);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDialSinkAddedCallback);
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

  // MediaSinkServiceBase implementation.
  void RecordDeviceCounts() override;

  // Connector to ServiceManager for safe XML parsing requests.
  std::unique_ptr<service_manager::Connector> connector_;

  std::unique_ptr<DeviceDescriptionService> description_service_;

  OnDialSinkAddedCallback dial_sink_added_cb_;

  // Raw pointer to DialRegistry singleton.
  DialRegistry* dial_registry_ = nullptr;

  // DialRegistry for unit test.
  DialRegistry* test_dial_registry_ = nullptr;

  // Device data list from current round of discovery.
  DialRegistry::DeviceList current_devices_;

  scoped_refptr<net::URLRequestContextGetter> request_context_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DialDeviceCountMetrics metrics_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_IMPL_H_
