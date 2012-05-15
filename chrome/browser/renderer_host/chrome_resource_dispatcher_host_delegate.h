// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#pragma once

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/metrics/variation_ids.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"

class DelayedResourceQueue;
class DownloadRequestLimiter;
class SafeBrowsingService;
class UserScriptListener;

namespace prerender {
class PrerenderTracker;
}

// Implements ResourceDispatcherHostDelegate. Currently used by the Prerender
// system to abort requests and add to the load flags when a request begins.
class ChromeResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate,
      public base::FieldTrialList::Observer {
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
      ResourceType::Type resource_type,
      content::ResourceContext* resource_context,
      const content::Referrer& referrer) OVERRIDE;
  virtual void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      ResourceType::Type resource_type,
      int child_id,
      int route_id,
      bool is_continuation_of_transferred_request,
      ScopedVector<content::ResourceThrottle>* throttles) OVERRIDE;
  virtual void DownloadStarting(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      int child_id,
      int route_id,
      int request_id,
      bool is_content_initiated,
      ScopedVector<content::ResourceThrottle>* throttles) OVERRIDE;
  virtual bool AcceptSSLClientCertificateRequest(
        net::URLRequest* request,
        net::SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual bool AcceptAuthRequest(net::URLRequest* request,
                                 net::AuthChallengeInfo* auth_info) OVERRIDE;
  virtual content::ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info, net::URLRequest* request) OVERRIDE;
  virtual void HandleExternalProtocol(const GURL& url,
                                      int child_id,
                                      int route_id) OVERRIDE;
  virtual bool ShouldForceDownloadResource(
      const GURL& url, const std::string& mime_type) OVERRIDE;
  virtual void OnResponseStarted(
      net::URLRequest* request,
      content::ResourceResponse* response,
      IPC::Message::Sender* sender) OVERRIDE;
  virtual void OnRequestRedirected(
      net::URLRequest* request,
      content::ResourceResponse* response) OVERRIDE;

  // base::FieldTrialList::Observer implementation.
  // This will add the variation ID associated with |trial_name| and
  // |group_name| to the variation ID cache.
  virtual void OnFieldTrialGroupFinalized(
      const std::string& trial_name,
      const std::string& group_name) OVERRIDE;

 private:
  void AppendStandardResourceThrottles(
      const net::URLRequest* request,
      content::ResourceContext* resource_context,
      int child_id,
      int route_id,
      ResourceType::Type resource_type,
      ScopedVector<content::ResourceThrottle>* throttles);

  // Adds Chrome experiment and metrics state as custom headers to |request|.
  // This is a best-effort attempt, and does not interrupt the request if the
  // headers can not be appended.
  void AppendChromeMetricsHeaders(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      ResourceType::Type resource_type);

  // Prepares the variation IDs cache with initial values if not already done.
  // This method also registers the caller with the FieldTrialList to receive
  // new variation IDs.
  void InitVariationIDsCacheIfNeeded();

  // Takes whatever is currently in |variation_ids_set_| and recreates
  // |variation_ids_header_| with it.
  void UpdateVariationIDsHeaderValue();

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;
  scoped_refptr<SafeBrowsingService> safe_browsing_;
  scoped_refptr<UserScriptListener> user_script_listener_;
  prerender::PrerenderTracker* prerender_tracker_;

  // Whether or not we've initialized the Cache.
  bool variation_ids_cache_initialized_;

  // Keep a cache of variation IDs that are transmitted in headers to Google.
  // This consists of a list of valid IDs, and the actual transmitted header.
  // Note that since this cache is both initialized and accessed from the IO
  // thread, we do not need to synchronize its uses.
  std::set<chrome_variations::ID> variation_ids_set_;
  std::string variation_ids_header_;

  DISALLOW_COPY_AND_ASSIGN(ChromeResourceDispatcherHostDelegate);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
