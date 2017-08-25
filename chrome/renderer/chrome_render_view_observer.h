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
#include "content/public/common/browser_controls_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "extensions/features/features.h"
#include "url/gurl.h"

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
  void Navigate(const GURL& url) override;
  void OnDestruct() override;

#if defined(OS_ANDROID)
  void OnUpdateBrowserControlsState(content::BrowserControlsState constraints,
                                    content::BrowserControlsState current,
                                    bool animate);
#endif

  // Determines if a host is in the strict security host set.
  bool IsStrictSecurityHost(const std::string& host);

  // Owned by ChromeContentRendererClient and outlive us.
  web_cache::WebCacheImpl* web_cache_impl_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderViewObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
