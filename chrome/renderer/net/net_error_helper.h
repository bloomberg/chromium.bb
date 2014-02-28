// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
#define CHROME_RENDERER_NET_NET_ERROR_HELPER_H_

#include <string>

#include "chrome/common/net/net_error_info.h"
#include "chrome/renderer/net/net_error_helper_core.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

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

 private:
  // NetErrorHelperCore::Delegate implementation:
  virtual void GenerateLocalizedErrorPage(const blink::WebURLError& error,
                                          bool is_failed_post,
                                          std::string* html) const OVERRIDE;
  virtual void LoadErrorPageInMainFrame(const std::string& html,
                                        const GURL& failed_url) OVERRIDE;
  virtual void UpdateErrorPage(const blink::WebURLError& error,
                               bool is_failed_post) OVERRIDE;
  virtual void FetchErrorPage(const GURL& url) OVERRIDE;
  virtual void CancelFetchErrorPage() OVERRIDE;

  void OnNetErrorInfo(int status);
  void OnSetAltErrorPageURL(const GURL& alternate_error_page_url);

  void OnAlternateErrorPageRetrieved(const blink::WebURLResponse& response,
                                     const std::string& data);

  scoped_ptr<content::ResourceFetcher> alt_error_page_fetcher_;

  NetErrorHelperCore core_;
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
