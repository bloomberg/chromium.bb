// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CHROME_BROWSER_LOADER_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/previews_state.h"
#include "extensions/features/features.h"

class DownloadRequestLimiter;

namespace content {
class NavigationData;
}

namespace extensions {
class UserScriptListener;
}

namespace net {
class URLRequest;
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
  void RequestBeginning(net::URLRequest* request,
                        content::ResourceContext* resource_context,
                        content::AppCacheService* appcache_service,
                        content::ResourceType resource_type,
                        std::vector<std::unique_ptr<content::ResourceThrottle>>*
                            throttles) override;
  void DownloadStarting(net::URLRequest* request,
                        content::ResourceContext* resource_context,
                        bool is_content_initiated,
                        bool must_download,
                        bool is_new_request,
                        std::vector<std::unique_ptr<content::ResourceThrottle>>*
                            throttles) override;
  content::ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      net::URLRequest* request) override;
  bool HandleExternalProtocol(const GURL& url,
                              content::ResourceRequestInfo* info) override;
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
                         content::ResourceResponse* response) override;
  void OnRequestRedirected(const GURL& redirect_url,
                           net::URLRequest* request,
                           content::ResourceContext* resource_context,
                           content::ResourceResponse* response) override;
  void RequestComplete(net::URLRequest* url_request) override;
  // Returns a bitmask of potentially several Previews optimizations.
  content::PreviewsState GetPreviewsState(
      const net::URLRequest& url_request,
      content::ResourceContext* resource_context) override;
  content::NavigationData* GetNavigationData(
      net::URLRequest* request) const override;
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      content::ResourceContext* resource_context) override;
  void OnAbortedFrameLoad(const GURL& url,
                          base::TimeDelta request_loading_time) override;

  // Called on the UI thread. Allows switching out the
  // ExternalProtocolHandler::Delegate for testing code.
  static void SetExternalProtocolHandlerDelegateForTesting(
      ExternalProtocolHandler::Delegate* delegate);

 protected:
  // Virtual for testing.
  virtual void AppendStandardResourceThrottles(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceType resource_type,
      std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles);

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  struct StreamTargetInfo {
    std::string extension_id;
    std::string view_id;
  };
#endif

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;
  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::UserScriptListener> user_script_listener_;
  std::map<net::URLRequest*, StreamTargetInfo> stream_target_info_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeResourceDispatcherHostDelegate);
};

#endif  // CHROME_BROWSER_LOADER_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
