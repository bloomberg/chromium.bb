// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "android_webview/renderer/aw_render_thread_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "components/web_restrictions/interfaces/web_restrictions.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
class SpellCheck;
#endif

namespace visitedlink {
class VisitedLinkSlave;
}

namespace android_webview {

class AwContentRendererClient : public content::ContentRendererClient,
                                public service_manager::LocalInterfaceProvider {
 public:
  AwContentRendererClient();
  ~AwContentRendererClient() override;

  // ContentRendererClient implementation.
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RenderViewCreated(content::RenderView* render_view) override;
  bool HasErrorPage(int http_status_code) override;
  void GetNavigationErrorStrings(content::RenderFrame* render_frame,
                                 const blink::WebURLRequest& failed_request,
                                 const blink::WebURLError& error,
                                 std::string* error_html,
                                 base::string16* error_description) override;
  unsigned long long VisitedLinkHash(const char* canonical_url,
                                     size_t length) override;
  bool IsLinkVisited(unsigned long long link_hash) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems)
      override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle() override;
  bool WillSendRequest(
      blink::WebLocalFrame* frame,
      ui::PageTransition transition_type,
      const blink::WebURL& url,
      std::vector<std::unique_ptr<content::URLLoaderThrottle>>* throttles,
      GURL* new_url) override;

  bool HandleNavigation(content::RenderFrame* render_frame,
                        bool is_content_initiated,
                        bool render_view_was_created_by_renderer,
                        blink::WebFrame* frame,
                        const blink::WebURLRequest& request,
                        blink::WebNavigationType type,
                        blink::WebNavigationPolicy default_policy,
                        bool is_redirect) override;
  bool ShouldUseMediaPlayerForURL(const GURL& url) override;

 private:
  // service_manager::LocalInterfaceProvider:
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override;

  // Returns |true| if we should use the SafeBrowsing mojo service. Initialises
  // |safe_browsing_| on the first call as a side-effect.
  bool UsingSafeBrowsingMojoService();

  std::unique_ptr<AwRenderThreadObserver> aw_render_thread_observer_;
  std::unique_ptr<visitedlink::VisitedLinkSlave> visited_link_slave_;
  web_restrictions::mojom::WebRestrictionsPtr web_restrictions_service_;
  safe_browsing::mojom::SafeBrowsingPtr safe_browsing_;

#if BUILDFLAG(ENABLE_SPELLCHECK)
  std::unique_ptr<SpellCheck> spellcheck_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AwContentRendererClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_
