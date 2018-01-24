// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view_extension.h"

#include "base/logging.h"
#include "chromecast/browser/cast_extension_host.h"
#include "extensions/browser/extension_system.h"

namespace chromecast {

CastWebViewExtension::CastWebViewExtension(
    const extensions::Extension* extension,
    const GURL& initial_url,
    CastWebView::Delegate* delegate,
    CastWebContentsManager* web_contents_manager,
    content::BrowserContext* browser_context,
    scoped_refptr<content::SiteInstance> site_instance,
    bool transparent,
    bool allow_media_access,
    bool is_headless,
    bool enable_touch_input)
    : window_(shell::CastContentWindow::Create(delegate,
                                               is_headless,
                                               enable_touch_input)),
      extension_host_(std::make_unique<CastExtensionHost>(
          browser_context,
          delegate,
          extension,
          initial_url,
          site_instance.get(),
          extensions::VIEW_TYPE_EXTENSION_POPUP)) {}

CastWebViewExtension::~CastWebViewExtension() {}

shell::CastContentWindow* CastWebViewExtension::window() const {
  return window_.get();
}

content::WebContents* CastWebViewExtension::web_contents() const {
  return extension_host_->host_contents();
}

void CastWebViewExtension::LoadUrl(GURL url) {
  extension_host_->CreateRenderViewSoon();
}

void CastWebViewExtension::ClosePage(const base::TimeDelta& shutdown_delay) {}

void CastWebViewExtension::Show(CastWindowManager* window_manager) {
  window_->ShowWebContents(web_contents(), window_manager);
  web_contents()->Focus();
}

}  // namespace chromecast
