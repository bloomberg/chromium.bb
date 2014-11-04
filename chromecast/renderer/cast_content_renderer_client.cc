// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include <sys/sysinfo.h>

#include "base/command_line.h"
#include "base/memory/memory_pressure_listener.h"
#include "chromecast/renderer/key_systems_cast.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_view.h"
#include "crypto/nss_util.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace chromecast {
namespace shell {

namespace {

// Default background color to set for WebViews. WebColor is in ARGB format
// though the comment of WebColor says it is in RGBA.
const blink::WebColor kColorBlack = 0xFF000000;

}  // namespace

void CastContentRendererClient::RenderThreadStarted() {
#if defined(USE_NSS)
  // Note: Copied from chrome_render_process_observer.cc to fix b/8676652.
  //
  // On platforms where the system NSS shared libraries are used,
  // initialize NSS now because it won't be able to load the .so's
  // after entering the sandbox.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    crypto::InitNSSSafely();
#endif
}

void CastContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  blink::WebView* webview = render_view->GetWebView();
  if (webview) {
    webview->setBaseBackgroundColor(kColorBlack);

    // The following settings express consistent behaviors across Cast
    // embedders, though Android has enabled by default for mobile browsers.
    webview->settings()->setShrinksViewportContentToFit(false);
    webview->settings()->setMediaControlsOverlayPlayButtonEnabled(false);
  }
}

void CastContentRendererClient::AddKeySystems(
    std::vector< ::media::KeySystemInfo>* key_systems) {
  AddChromecastKeySystems(key_systems);
  AddChromecastPlatformKeySystems(key_systems);
}

}  // namespace shell
}  // namespace chromecast
