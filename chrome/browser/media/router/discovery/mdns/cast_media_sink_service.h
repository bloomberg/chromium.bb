// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_delegate.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service.h"
#include "content/public/browser/browser_thread.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace media_router {

class CastMediaSinkServiceImpl;

// A service which can be used to start background discovery and resolution of
// Cast devices.
// Public APIs should be invoked on the UI thread.
class CastMediaSinkService
    : public MediaSinkService,
      public DialMediaSinkServiceObserver,
      public DnsSdRegistry::DnsSdObserver,
      public base::RefCountedThreadSafe<CastMediaSinkService> {
 public:
  CastMediaSinkService(
      const OnSinksDiscoveredCallback& callback,
      content::BrowserContext* browser_context,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // Used by unit tests.
  CastMediaSinkService(
      const OnSinksDiscoveredCallback& callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      std::unique_ptr<CastMediaSinkServiceImpl,
                      content::BrowserThread::DeleteOnIOThread>
          cast_media_sink_service_impl);

  // mDNS service types.
  static const char kCastServiceType[];

  // MediaSinkService implementation
  void Start() override;
  void Stop() override;
  void ForceSinkDiscoveryCallback() override;

  void SetDnsSdRegistryForTest(DnsSdRegistry* registry);

  // Forces a mDNS discovery if the service has been started; No-op otherwise.
  // It is invoked by OnUserGesture().
  void ForceDiscovery();

 protected:
  ~CastMediaSinkService() override;

 private:
  friend class base::RefCountedThreadSafe<CastMediaSinkService>;
  friend class CastMediaSinkServiceTest;

  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestRestartAfterStop);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestMultipleStartAndStop);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestOnDnsSdEvent);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestTimer);

  // Callback passed to CastMediaSinkServiceImpl class to be invoked when timer
  // expires.
  void OnSinksDiscoveredOnIOThread(std::vector<MediaSinkInternal> sinks);

  // DnsSdRegistry::DnsSdObserver implementation
  void OnDnsSdEvent(const std::string& service_type,
                    const DnsSdRegistry::DnsSdServiceList& services) override;

  // DialMediaSinkServiceObserver implementation
  void OnDialSinkAdded(const MediaSinkInternal& sink) override;

  // Raw pointer to DnsSdRegistry instance, which is a global leaky singleton
  // and lives as long as the browser process.
  DnsSdRegistry* dns_sd_registry_ = nullptr;

  // Task runner for the IO thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Created on the UI thread and destroyed on the IO thread.
  std::unique_ptr<CastMediaSinkServiceImpl,
                  content::BrowserThread::DeleteOnIOThread>
      cast_media_sink_service_impl_;

  // List of cast sinks found in current round of mDNS discovery.
  std::vector<MediaSinkInternal> cast_sinks_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkService);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_H_
