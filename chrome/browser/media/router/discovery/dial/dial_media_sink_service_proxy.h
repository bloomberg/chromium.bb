// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_PROXY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/common/media_router/discovery/media_sink_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {
class BrowserContext;
}

namespace net {
class URLRequestContextGetter;
}

namespace media_router {

class DialMediaSinkServiceImpl;
class DialMediaSinkServiceObserver;

// A wrapper class of DialMediaSinkService handling thread hopping between UI
// and IO threads. This class is thread safe. Public APIs should be invoked on
// UI thread. It then post tasks to IO thread and invoke them on underlying
// DialMediaSinkService instance.
class DialMediaSinkServiceProxy
    : public MediaSinkService,
      public base::RefCountedThreadSafe<
          DialMediaSinkServiceProxy,
          content::BrowserThread::DeleteOnIOThread> {
 public:
  // |callback| is invoked on the UI thread when sinks are discovered.
  // |context| is browser context.
  DialMediaSinkServiceProxy(
      const MediaSinkService::OnSinksDiscoveredCallback& callback,
      content::BrowserContext* context);

  // Starts discovery of DIAL devices on IO thread. Caller is responsible for
  // calling Stop() before destroying this object..
  void Start() override;

  // Stops discovery of DIAL devices on IO thread. The callback passed to
  // Start() is cleared.
  void Stop() override;

  // MediaSinkService implementation.
  void ForceSinkDiscoveryCallback() override;
  void OnUserGesture() override;

  // Does not take ownership of |observer|. Caller should make sure |observer|
  // object outlives |this|.
  void SetObserver(DialMediaSinkServiceObserver* observer);

  // Sets |observer_| to nullptr.
  void ClearObserver();

  void SetDialMediaSinkServiceForTest(
      std::unique_ptr<DialMediaSinkServiceImpl> dial_media_sink_service);

 private:
  friend class DialMediaSinkServiceProxyTest;
  friend class base::DeleteHelper<DialMediaSinkServiceProxy>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::RefCountedThreadSafe<DialMediaSinkServiceProxy>;

  ~DialMediaSinkServiceProxy() override;

  // Starts DIAL discovery.
  void StartOnIOThread();

  // Stops DIAL discovery.
  void StopOnIOThread();

  // Callback passed to |dial_media_sink_service_| ctor. When invoked, post task
  // to UI thread and invoke |sink_discovery_callback_|.
  void OnSinksDiscoveredOnIOThread(std::vector<MediaSinkInternal> sinks);

 private:
  std::unique_ptr<DialMediaSinkServiceImpl> dial_media_sink_service_;

  DialMediaSinkServiceObserver* observer_;

  scoped_refptr<net::URLRequestContextGetter> request_context_;

  DISALLOW_COPY_AND_ASSIGN(DialMediaSinkServiceProxy);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_PROXY_H_
