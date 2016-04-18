// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
#define CHROME_RENDERER_NET_NET_ERROR_HELPER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/renderer/net/net_error_page_controller.h"
#include "components/error_page/common/net_error_info.h"
#include "components/error_page/renderer/net_error_helper_core.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_thread_observer.h"

class GURL;

namespace blink {
class WebFrame;
class WebURLResponse;
struct WebURLError;
}

namespace content {
class ResourceFetcher;
}

namespace error_page {
struct ErrorPageParams;
}

// Listens for NetErrorInfo messages from the NetErrorTabHelper on the
// browser side and updates the error page with more details (currently, just
// DNS probe results) if/when available.
// TODO(crbug.com/578770): Should this class be moved into the error_page
// component?
class NetErrorHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<NetErrorHelper>,
      public content::RenderThreadObserver,
      public error_page::NetErrorHelperCore::Delegate,
      public NetErrorPageController::Delegate {
 public:
  explicit NetErrorHelper(content::RenderFrame* render_frame);
  ~NetErrorHelper() override;

  // NetErrorPageController::Delegate implementation
  void ButtonPressed(error_page::NetErrorHelperCore::Button button) override;
  void TrackClick(int tracking_id) override;

  // RenderFrameObserver implementation.
  void DidStartProvisionalLoad() override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;
  void DidFinishLoad() override;
  void OnStop() override;
  void WasShown() override;
  void WasHidden() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // RenderThreadObserver implementation.
  void NetworkStateChanged(bool online) override;

  // Initializes |error_html| with the HTML of an error page in response to
  // |error|.  Updates internals state with the assumption the page will be
  // loaded immediately.
  void GetErrorHTML(const blink::WebURLError& error,
                    bool is_failed_post,
                    bool is_ignoring_cache,
                    std::string* error_html);

  // Returns whether a load for |url| in the |frame| the NetErrorHelper is
  // attached to should have its error page suppressed.
  bool ShouldSuppressErrorPage(const GURL& url);

 private:
  // NetErrorHelperCore::Delegate implementation:
  void GenerateLocalizedErrorPage(
      const blink::WebURLError& error,
      bool is_failed_post,
      bool can_use_local_diagnostics_service,
      bool has_offline_pages,
      std::unique_ptr<error_page::ErrorPageParams> params,
      bool* reload_button_shown,
      bool* show_saved_copy_button_shown,
      bool* show_cached_copy_button_shown,
      bool* show_offline_pages_button_shown,
      std::string* html) const override;
  void LoadErrorPage(const std::string& html, const GURL& failed_url) override;
  void EnablePageHelperFunctions() override;
  void UpdateErrorPage(const blink::WebURLError& error,
                       bool is_failed_post,
                       bool can_use_local_diagnostics_service,
                       bool has_offline_pages) override;
  void FetchNavigationCorrections(
      const GURL& navigation_correction_url,
      const std::string& navigation_correction_request_body) override;
  void CancelFetchNavigationCorrections() override;
  void SendTrackingRequest(const GURL& tracking_url,
                           const std::string& tracking_request_body) override;
  void ReloadPage(bool bypass_cache) override;
  void LoadPageFromCache(const GURL& page_url) override;
  void DiagnoseError(const GURL& page_url) override;
  void ShowOfflinePages() override;

  void OnNetErrorInfo(int status);
  void OnSetCanShowNetworkDiagnosticsDialog(
      bool can_use_local_diagnostics_service);
  void OnSetNavigationCorrectionInfo(const GURL& navigation_correction_url,
                                     const std::string& language,
                                     const std::string& country_code,
                                     const std::string& api_key,
                                     const GURL& search_url);

  void OnNavigationCorrectionsFetched(const blink::WebURLResponse& response,
                                      const std::string& data);

  void OnTrackingRequestComplete(const blink::WebURLResponse& response,
                                 const std::string& data);

#if defined(OS_ANDROID)
  // Called to set whether offline pages exists, which will be used to decide
  // if offline related button will be provided in the error page.
  void OnSetHasOfflinePages(bool has_offline_pages);
#endif

  std::unique_ptr<content::ResourceFetcher> correction_fetcher_;
  std::unique_ptr<content::ResourceFetcher> tracking_fetcher_;

  std::unique_ptr<error_page::NetErrorHelperCore> core_;

  // Weak factory for vending a weak pointer to a NetErrorPageController. Weak
  // pointers are invalidated on each commit, to prevent getting messages from
  // Controllers used for the previous commit that haven't yet been cleaned up.
  base::WeakPtrFactory<NetErrorPageController::Delegate>
      weak_controller_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetErrorHelper);
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
