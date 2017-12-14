// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_delegate.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service_util.h"
#include "content/public/browser/browser_thread.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace media_router {

class CastMediaSinkServiceImpl;

// A service which can be used to start background discovery and resolution of
// Cast devices.
// This class is not thread safe. All methods must be invoked on the UI thread.
class CastMediaSinkService : public DnsSdRegistry::DnsSdObserver {
 public:
  // mDNS service types.
  static const char kCastServiceType[];

  explicit CastMediaSinkService(content::BrowserContext* browser_context);

  ~CastMediaSinkService() override;

  // Returns a callback to |impl_| when a DIAL sink is added (e.g., in order
  // to perform dual discovery). The callback must be run on the sequence given
  // by |GetImplTaskRunner()|. It is safe to invoke this callback after |this|
  // is destroyed.
  // TODO(imcheng): It would be better to have this callback PostTask to the
  // correct SequencedTaskRunner rather than making the caller do it.
  OnDialSinkAddedCallback GetDialSinkAddedCallback();

  // Returns |impl_|'s task runner to be used with |GetDialSinkAddedCallback()|.
  scoped_refptr<base::SequencedTaskRunner> GetImplTaskRunner();

  // Starts Cast sink discovery. No-ops if already started.
  // |sink_discovery_cb|: Callback to invoke when the list of discovered sinks
  // has been updated. The callback is invoked on the UI thread and may be
  // invoked after |this| is destroyed.
  // Marked virtual for tests.
  virtual void Start(const OnSinksDiscoveredCallback& sinks_discovered_cb);

  // Forces the sink discovery callback to be invoked with the current list of
  // sinks.
  // Marked virtual for tests.
  virtual void ForceSinkDiscoveryCallback();

  // Initiates discovery immediately in response to a user gesture
  // (i.e., opening the Media Router dialog).
  // TODO(imcheng): Rename to ManuallyInitiateDiscovery() or similar.
  // Marked virtual for tests.
  virtual void OnUserGesture();

  // Marked virtual for tests.
  virtual std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
  CreateImpl(const OnSinksDiscoveredCallback& sinks_discovered_cb);

  // Registers with DnsSdRegistry to listen for Cast devices. Note that this is
  // called on |Start()| on all platforms except for Windows. On Windows, this
  // method should be invoked either if enabling mDNS will not trigger a
  // firewall prompt, or if the resulting firewall prompt can be associated with
  // a user gesture (e.g. opening the Media Router dialog).
  // Subsequent invocations of this method are no-op.
  // Marked virtual for tests.
  virtual void StartMdnsDiscovery();

  void SetDnsSdRegistryForTest(DnsSdRegistry* registry);

 private:
  friend class CastMediaSinkServiceTest;

  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestRestartAfterStop);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestMultipleStartAndStop);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestOnDnsSdEvent);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestTimer);

  void OnDialSinkAdded(const MediaSinkInternal& sink);

  // DnsSdRegistry::DnsSdObserver implementation
  void OnDnsSdEvent(const std::string& service_type,
                    const DnsSdRegistry::DnsSdServiceList& services) override;

  // Raw pointer to DnsSdRegistry instance, which is a global leaky singleton
  // and lives as long as the browser process.
  DnsSdRegistry* dns_sd_registry_ = nullptr;

  // Created on the UI thread, used and destroyed on its SequencedTaskRunner.
  std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter> impl_;

  // List of cast sinks found in current round of mDNS discovery.
  std::vector<MediaSinkInternal> cast_sinks_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkService);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_H_
