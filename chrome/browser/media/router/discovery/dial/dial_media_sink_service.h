// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_

#include <memory>
#include <set>

#include "chrome/browser/media/router/discovery/dial/device_description_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_registry.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service.h"

namespace media_router {

class DeviceDescriptionService;
class DialRegistry;

// A service which can be used to start background discovery and resolution of
// DIAL devices (Smart TVs, Game Consoles, etc.).
// This class is not thread safe. All methods must be called from the IO thread.
class DialMediaSinkService : public MediaSinkService,
                             public DialRegistry::Observer {
 public:
  DialMediaSinkService(const OnSinksDiscoveredCallback& callback,
                       net::URLRequestContextGetter* request_context);
  ~DialMediaSinkService() override;

  // Stops listening for DIAL device events.
  virtual void Stop();

  // MediaSinkService implementation
  void Start() override;

 protected:

  // Returns instance of device description service. Create a new one if none
  // exists.
  DeviceDescriptionService* GetDescriptionService();

  // Does not take ownership of |dial_registry|.
  void SetDialRegistryForTest(DialRegistry* dial_registry);
  void SetDescriptionServiceForTest(
      std::unique_ptr<DeviceDescriptionService> description_service);
  void SetTimerForTest(std::unique_ptr<base::Timer> timer);

 private:
  friend class DialMediaSinkServiceTest;
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceTest, TestStart);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceTest, TestTimer);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceTest,
                           TestFetchCompleted_SameSink);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceTest, TestIsDifferent);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceTest,
                           TestOnDeviceDescriptionAvailable);

  // api::dial::DialRegistry::Observer implementation
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

  // Called when |finish_timer_| expires.
  void OnFetchCompleted();

  // Helper function to start |finish_timer_|.
  void StartTimer();

  // Timer for finishing fetching. Starts in |OnDialDeviceEvent()|, and expires
  // 3 seconds later. If |OnDeviceDescriptionAvailable()| is called after
  // |finish_timer_| expires, |finish_timer_| is restarted.
  std::unique_ptr<base::Timer> finish_timer_;

  std::unique_ptr<DeviceDescriptionService> description_service_;

  // Raw pointer to DialRegistry singleton.
  DialRegistry* dial_registry_ = nullptr;

  // Sorted sinks from current round of discovery.
  std::set<MediaSinkInternal> current_sinks_;

  // Sorted sinks sent to Media Router Provider in last FetchCompleted().
  std::set<MediaSinkInternal> mrp_sinks_;

  // Device data list from current round of discovery.
  DialRegistry::DeviceList current_devices_;

  scoped_refptr<net::URLRequestContextGetter> request_context_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_H_
