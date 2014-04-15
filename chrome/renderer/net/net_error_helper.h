// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
#define CHROME_RENDERER_NET_NET_ERROR_HELPER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/localized_error.h"
#include "chrome/common/net/net_error_info.h"
#include "chrome/renderer/net/net_error_helper_core.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_process_observer.h"

class GURL;

namespace content {
class ResourceFetcher;
}

namespace blink {
class WebFrame;
class WebURLResponse;
struct WebURLError;
}

// Listens for NetErrorInfo messages from the NetErrorTabHelper on the
// browser side and updates the error page with more details (currently, just
// DNS probe results) if/when available.
class NetErrorHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<NetErrorHelper>,
      public content::RenderProcessObserver,
      public NetErrorHelperCore::Delegate {
 public:
  explicit NetErrorHelper(content::RenderFrame* render_view);
  virtual ~NetErrorHelper();

  // RenderFrameObserver implementation.
  virtual void DidStartProvisionalLoad() OVERRIDE;
  virtual void DidCommitProvisionalLoad(bool is_new_navigation) OVERRIDE;
  virtual void DidFinishLoad() OVERRIDE;
  virtual void OnStop() OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // RenderProcessObserver implementation.
  virtual void NetworkStateChanged(bool online) OVERRIDE;

  // Examines |frame| and |error| to see if this is an error worthy of a DNS
  // probe.  If it is, initializes |error_strings| based on |error|,
  // |is_failed_post|, and |locale| with suitable strings and returns true.
  // If not, returns false, in which case the caller should look up error
  // strings directly using LocalizedError::GetNavigationErrorStrings.
  //
  // Updates the NetErrorHelper with the assumption the page will be loaded
  // immediately.
  void GetErrorHTML(blink::WebFrame* frame,
                    const blink::WebURLError& error,
                    bool is_failed_post,
                    std::string* error_html);

  // Returns whether a load for |url| in |frame| should have its error page
  // suppressed.
  bool ShouldSuppressErrorPage(blink::WebFrame* frame, const GURL& url);

 private:
  // NetErrorHelperCore::Delegate implementation:
  virtual void GenerateLocalizedErrorPage(
      const blink::WebURLError& error,
      bool is_failed_post,
      scoped_ptr<LocalizedError::ErrorPageParams> params,
      std::string* html) const OVERRIDE;
  virtual void LoadErrorPageInMainFrame(const std::string& html,
                                        const GURL& failed_url) OVERRIDE;
  virtual void EnableStaleLoadBindings(const GURL& page_url) OVERRIDE;
  virtual void UpdateErrorPage(const blink::WebURLError& error,
                               bool is_failed_post) OVERRIDE;
  virtual void FetchNavigationCorrections(
      const GURL& navigation_correction_url,
      const std::string& navigation_correction_request_body) OVERRIDE;
  virtual void CancelFetchNavigationCorrections() OVERRIDE;
  virtual void ReloadPage() OVERRIDE;

  void OnNetErrorInfo(int status);
  void OnSetNavigationCorrectionInfo(const GURL& navigation_correction_url,
                                     const std::string& language,
                                     const std::string& country_code,
                                     const std::string& api_key,
                                     const GURL& search_url);

  void OnNavigationCorrectionsFetched(const blink::WebURLResponse& response,
                                      const std::string& data);

  scoped_ptr<content::ResourceFetcher> correction_fetcher_;

  NetErrorHelperCore core_;

  DISALLOW_COPY_AND_ASSIGN(NetErrorHelper);
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
