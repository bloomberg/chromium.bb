// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include <map>
#include <memory>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"

class DelayedResourceQueue;
class DownloadRequestLimiter;

namespace extensions {
class UserScriptListener;
}

namespace safe_browsing {
class SafeBrowsingService;
}

// Implements ResourceDispatcherHostDelegate. Currently used by the Prerender
// system to abort requests and add to the load flags when a request begins.
class ChromeResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate {
 public:
  ChromeResourceDispatcherHostDelegate();
  ~ChromeResourceDispatcherHostDelegate() override;

  // ResourceDispatcherHostDelegate implementation.
  bool ShouldBeginRequest(const std::string& method,
                          const GURL& url,
                          content::ResourceType resource_type,
                          content::ResourceContext* resource_context) override;
  void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::AppCacheService* appcache_service,
      content::ResourceType resource_type,
      ScopedVector<content::ResourceThrottle>* throttles) override;
  void DownloadStarting(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      int child_id,
      int route_id,
      int request_id,
      bool is_content_initiated,
      bool must_download,
      ScopedVector<content::ResourceThrottle>* throttles) override;
  content::ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      net::URLRequest* request) override;
  bool HandleExternalProtocol(
      const GURL& url,
      int child_id,
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture) override;
  bool ShouldForceDownloadResource(const GURL& url,
                                   const std::string& mime_type) override;
  bool ShouldInterceptResourceAsStream(net::URLRequest* request,
                                       const base::FilePath& plugin_path,
                                       const std::string& mime_type,
                                       GURL* origin,
                                       std::string* payload) override;
  void OnStreamCreated(net::URLRequest* request,
                       std::unique_ptr<content::StreamInfo> stream) override;
  void OnResponseStarted(net::URLRequest* request,
                         content::ResourceContext* resource_context,
                         content::ResourceResponse* response,
                         IPC::Sender* sender) override;
  void OnRequestRedirected(const GURL& redirect_url,
                           net::URLRequest* request,
                           content::ResourceContext* resource_context,
                           content::ResourceResponse* response) override;
  void RequestComplete(net::URLRequest* url_request) override;
  bool ShouldEnableLoFiMode(
      const net::URLRequest& url_request,
      content::ResourceContext* resource_context) override;

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
      content::ResourceType resource_type,
      ScopedVector<content::ResourceThrottle>* throttles);

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;
  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::UserScriptListener> user_script_listener_;
  std::map<net::URLRequest*, StreamTargetInfo> stream_target_info_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeResourceDispatcherHostDelegate);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
