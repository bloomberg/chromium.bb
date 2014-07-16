// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"

class DelayedResourceQueue;
class DownloadRequestLimiter;
class SafeBrowsingService;

namespace extensions {
class UserScriptListener;
}

namespace prerender {
class PrerenderTracker;
}

// Implements ResourceDispatcherHostDelegate. Currently used by the Prerender
// system to abort requests and add to the load flags when a request begins.
class ChromeResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate {
 public:
  // This class does not take ownership of the tracker but merely holds a
  // reference to it to avoid accessing g_browser_process.
  // |prerender_tracker| must outlive |this|.
  explicit ChromeResourceDispatcherHostDelegate(
      prerender::PrerenderTracker* prerender_tracker);
  virtual ~ChromeResourceDispatcherHostDelegate();

  // ResourceDispatcherHostDelegate implementation.
  virtual bool ShouldBeginRequest(
      int child_id,
      int route_id,
      const std::string& method,
      const GURL& url,
      content::ResourceType::Type resource_type,
      content::ResourceContext* resource_context) OVERRIDE;
  virtual void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::AppCacheService* appcache_service,
      content::ResourceType::Type resource_type,
      int child_id,
      int route_id,
      ScopedVector<content::ResourceThrottle>* throttles) OVERRIDE;
  virtual void DownloadStarting(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      int child_id,
      int route_id,
      int request_id,
      bool is_content_initiated,
      bool must_download,
      ScopedVector<content::ResourceThrottle>* throttles) OVERRIDE;
  virtual content::ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info, net::URLRequest* request) OVERRIDE;
  virtual bool HandleExternalProtocol(const GURL& url,
                                      int child_id,
                                      int route_id,
                                      bool initiated_by_user_gesture) OVERRIDE;
  virtual bool ShouldForceDownloadResource(
      const GURL& url, const std::string& mime_type) OVERRIDE;
  virtual bool ShouldInterceptResourceAsStream(
      net::URLRequest* request,
      const std::string& mime_type,
      GURL* origin,
      std::string* payload) OVERRIDE;
  virtual void OnStreamCreated(
      net::URLRequest* request,
      scoped_ptr<content::StreamHandle> stream) OVERRIDE;
  virtual void OnResponseStarted(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceResponse* response,
      IPC::Sender* sender) OVERRIDE;
  virtual void OnRequestRedirected(
      const GURL& redirect_url,
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceResponse* response) OVERRIDE;
  virtual void RequestComplete(net::URLRequest* url_request) OVERRIDE;

  // Called on the UI thread. Allows switching out the
  // ExternalProtocolHandler::Delegate for testing code.
  static void SetExternalProtocolHandlerDelegateForTesting(
      ExternalProtocolHandler::Delegate* delegate);

 private:
#if defined(ENABLE_EXTENSIONS)
  struct StreamTargetInfo {
    std::string extension_id;
    std::string view_id;
  };
#endif

  void AppendStandardResourceThrottles(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceType::Type resource_type,
      ScopedVector<content::ResourceThrottle>* throttles);

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // Append headers required to tell Gaia whether the sync interstitial
  // should be shown or not.  This header is only added for valid Gaia URLs.
  void AppendChromeSyncGaiaHeader(
      net::URLRequest* request,
      content::ResourceContext* resource_context);
#endif

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;
  scoped_refptr<SafeBrowsingService> safe_browsing_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::UserScriptListener> user_script_listener_;
  std::map<net::URLRequest*, StreamTargetInfo> stream_target_info_;
#endif
  prerender::PrerenderTracker* prerender_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ChromeResourceDispatcherHostDelegate);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
