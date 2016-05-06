// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/public/common/top_controls_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "url/gurl.h"

class ContentSettingsObserver;
class SkBitmap;

namespace blink {
class WebView;
struct WebWindowFeatures;
}

namespace web_cache {
class WebCacheImpl;
}

// This class holds the Chrome specific parts of RenderView, and has the same
// lifetime.
class ChromeRenderViewObserver : public content::RenderViewObserver {
 public:
  // translate_helper can be NULL.
  ChromeRenderViewObserver(
      content::RenderView* render_view,
      web_cache::WebCacheImpl* web_cache_render_thread_observer);
  ~ChromeRenderViewObserver() override;

 private:
  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidCommitProvisionalLoad(blink::WebLocalFrame* frame,
                                bool is_new_navigation) override;
  void Navigate(const GURL& url) override;

#if !defined(OS_ANDROID)
  void OnWebUIJavaScript(const base::string16& javascript);
#endif
#if defined(ENABLE_EXTENSIONS)
  void OnSetVisuallyDeemphasized(bool deemphasized);
#endif
#if defined(OS_ANDROID)
  void OnUpdateTopControlsState(content::TopControlsState constraints,
                                content::TopControlsState current,
                                bool animate);
#endif
  void OnGetWebApplicationInfo();
  void OnSetWindowFeatures(const blink::WebWindowFeatures& window_features);

  // Determines if a host is in the strict security host set.
  bool IsStrictSecurityHost(const std::string& host);

  // Save the JavaScript to preload if a ViewMsg_WebUIJavaScript is received.
  std::vector<base::string16> webui_javascript_;

  // Owned by ChromeContentRendererClient and outlive us.
  web_cache::WebCacheImpl* web_cache_impl_;

  // true if webview is overlayed with grey color.
  bool webview_visually_deemphasized_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderViewObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
