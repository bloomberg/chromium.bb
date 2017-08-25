// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_view_observer.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/features/features.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/chrome_extension_messages.h"
#endif

using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebWindowFeatures;

ChromeRenderViewObserver::ChromeRenderViewObserver(
    content::RenderView* render_view,
    web_cache::WebCacheImpl* web_cache_impl)
    : content::RenderViewObserver(render_view),
      web_cache_impl_(web_cache_impl) {}

ChromeRenderViewObserver::~ChromeRenderViewObserver() {
}

bool ChromeRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
#if defined(OS_ANDROID)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_UpdateBrowserControlsState,
                        OnUpdateBrowserControlsState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
#else
  return false;
#endif
}

#if defined(OS_ANDROID)
void ChromeRenderViewObserver::OnUpdateBrowserControlsState(
    content::BrowserControlsState constraints,
    content::BrowserControlsState current,
    bool animate) {
  render_view()->UpdateBrowserControlsState(constraints, current, animate);
}
#endif

void ChromeRenderViewObserver::Navigate(const GURL& url) {
  // Execute cache clear operations that were postponed until a navigation
  // event (including tab reload).
  if (web_cache_impl_)
    web_cache_impl_->ExecutePendingClearCache();
}

void ChromeRenderViewObserver::OnDestruct() {
  delete this;
}
