// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"

#include "android_webview/renderer/aw_render_process_observer.h"
#include "base/compiler_specific.h"

namespace visitedlink {
class VisitedLinkSlave;
}

namespace android_webview {

class AwContentRendererClient : public content::ContentRendererClient {
 public:
  AwContentRendererClient();
  virtual ~AwContentRendererClient();

  // ContentRendererClient implementation.
  virtual void RenderThreadStarted() OVERRIDE;
  virtual void RenderFrameCreated(content::RenderFrame* render_frame) OVERRIDE;
  virtual void RenderViewCreated(content::RenderView* render_view) OVERRIDE;
  virtual std::string GetDefaultEncoding() OVERRIDE;
  virtual bool HasErrorPage(int http_status_code,
                            std::string* error_domain) OVERRIDE;
  virtual void GetNavigationErrorStrings(
      content::RenderView* render_view,
      blink::WebFrame* frame,
      const blink::WebURLRequest& failed_request,
      const blink::WebURLError& error,
      std::string* error_html,
      base::string16* error_description) OVERRIDE;
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length) OVERRIDE;
  virtual bool IsLinkVisited(unsigned long long link_hash) OVERRIDE;
  virtual void AddKeySystems(
      std::vector<content::KeySystemInfo>* key_systems) OVERRIDE;

  virtual bool HandleNavigation(content::RenderFrame* render_frame,
                                content::DocumentState* document_state,
                                int opener_id,
                                blink::WebFrame* frame,
                                const blink::WebURLRequest& request,
                                blink::WebNavigationType type,
                                blink::WebNavigationPolicy default_policy,
                                bool is_redirect) OVERRIDE;

 private:
  scoped_ptr<AwRenderProcessObserver> aw_render_process_observer_;
  scoped_ptr<visitedlink::VisitedLinkSlave> visited_link_slave_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_
