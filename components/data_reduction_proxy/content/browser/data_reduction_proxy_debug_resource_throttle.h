// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_DEBUG_RESOURCE_THROTTLE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_DEBUG_RESOURCE_THROTTLE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_ui_manager.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace data_reduction_proxy {

class DataReductionProxyConfig;
class DataReductionProxyDebugUIService;
class DataReductionProxyIOData;

// Gets notified at various points during the process of loading a resource.
// Defers the resource load when the Data Reduction Proxy is not going to be
// used. Resumes or cancels a deferred resource load via the ResourceController.
//
// Displays a warning when the proxy will not be used for a request because it
// isn't configured (a previous bypass) and when the proxy is supposed to be
// used, but there is a connection error. Will not warn when the Data Reduction
// Proxy is bypassed by local rules, or when the url scheme is not proxied by
// the Data Reduction Proxy.
//
// This class is useful, e.g., for users that wish to be warned when
// configurations that are able to go through the Data Reduction Proxy are
// bypassed.
//
// TODO(megjablon): An interstitial cannot be displayed at the time of
// WillProcessResponse. This prevents warning at the time when the Data
// Reduction Proxy returns a response that triggers a bypass. Therefore this
// interstitial is not shown for block=once bypasses. For other bypasses, the
// interstitial is not not shown on the initial bypassed request, but on the
// subsequent request.
class DataReductionProxyDebugResourceThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<DataReductionProxyDebugResourceThrottle> {
 public:
  static scoped_ptr<DataReductionProxyDebugResourceThrottle> MaybeCreate(
      const net::URLRequest* request,
      content::ResourceType resource_type,
      const DataReductionProxyIOData* io_data);

  // Starts displaying the Data Reduction Proxy blocking page if it is not
  // prerendering. Must only be called on the UI thread. Takes a bypass
  // resource, which is a structure used to pass parameters between the IO and
  // UI thread, and shows a DataReductionBlockingPage for the resource using
  // the given ui_manager.
  static void StartDisplayingBlockingPage(
      scoped_refptr<DataReductionProxyDebugUIManager> ui_manager,
      const DataReductionProxyDebugUIManager::BypassResource& resource);

  // Constructs a DataReductionProxyDebugResourceThrottle object with the given
  // request, resource type, ui service, and config.
  DataReductionProxyDebugResourceThrottle(
      const net::URLRequest* request,
      content::ResourceType resource_type,
      const DataReductionProxyDebugUIService* ui_service,
      const DataReductionProxyConfig* config);

  ~DataReductionProxyDebugResourceThrottle() override;

  // content::ResourceThrottle overrides:
  void WillStartUsingNetwork(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  const char* GetNameForLogging() const override;

 private:
  enum State {
    NOT_BYPASSED,   // A warning has not been shown yet for this request.
    LOCAL_BYPASS,   // The request was bypassed by local bypass rules.
    REMOTE_BYPASS,  // The request was bypassed by the Data Reduction Proxy.
  };

  // Creates a bypass resource and calls StartDisplayingBlockingPage on the UI
  // thread. Sets defer to true. A bypass resource is a structure used to pass
  // parameters between the IO and UI thread when interacting with the
  // interstitial. Virtual for testing.
  virtual void DisplayBlockingPage(bool* defer);

  // Called on the IO thread when the user has decided to proceed with the
  // current request, or go back.
  void OnBlockingPageComplete(bool proceed);

  // The bypass state of this.
  State state_;

  // Must outlive |this|.
  const net::URLRequest* request_;

  // Holds the UI manager that shows interstitials. Must outlive |this|.
  const DataReductionProxyDebugUIService* ui_service_;

  // Must outlive |this|.
  const DataReductionProxyConfig* config_;

  const bool is_subresource_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyDebugResourceThrottle);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_DEBUG_RESOURCE_THROTTLE_H_
