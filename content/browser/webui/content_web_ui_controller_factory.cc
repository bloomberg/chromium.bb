// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/content_web_ui_controller_factory.h"

#include "content/browser/gpu/gpu_internals_ui.h"
#include "content/browser/media/media_internals_ui.h"
#include "content/browser/media/webrtc_internals_ui.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"

namespace content {

WebUI::TypeID ContentWebUIControllerFactory::GetWebUIType(
      BrowserContext* browser_context, const GURL& url) const {
  if (url.host() == chrome::kChromeUIWebRTCInternalsHost ||
#if !defined(OS_ANDROID)
      url.host() == chrome::kChromeUITracingHost ||
#endif
      url.host() == chrome::kChromeUIGpuHost ||
      url.host() == chrome::kChromeUIMediaInternalsHost) {
    return const_cast<ContentWebUIControllerFactory*>(this);
  }
  return WebUI::kNoWebUI;
}

bool ContentWebUIControllerFactory::UseWebUIForURL(
    BrowserContext* browser_context, const GURL& url) const {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

bool ContentWebUIControllerFactory::UseWebUIBindingsForURL(
    BrowserContext* browser_context, const GURL& url) const {
  return UseWebUIForURL(browser_context, url);
}

WebUIController* ContentWebUIControllerFactory::CreateWebUIControllerForURL(
    WebUI* web_ui, const GURL& url) const {
  if (url.host() == chrome::kChromeUIWebRTCInternalsHost)
    return new WebRTCInternalsUI(web_ui);
  if (url.host() == chrome::kChromeUIGpuHost)
    return new GpuInternalsUI(web_ui);
  if (url.host() == chrome::kChromeUIMediaInternalsHost)
    return new MediaInternalsUI(web_ui);
#if !defined(OS_ANDROID)
  if (url.host() == chrome::kChromeUITracingHost)
    return new TracingUI(web_ui);
#endif

  return NULL;
}

// static
ContentWebUIControllerFactory* ContentWebUIControllerFactory::GetInstance() {
  return Singleton<ContentWebUIControllerFactory>::get();
}

ContentWebUIControllerFactory::ContentWebUIControllerFactory() {
}

ContentWebUIControllerFactory::~ContentWebUIControllerFactory() {
}

}  // namespace content
