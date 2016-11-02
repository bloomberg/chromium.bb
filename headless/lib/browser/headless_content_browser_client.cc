// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_content_browser_client.h"

#include <memory>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/service_names.h"
#include "headless/grit/headless_lib_resources.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_devtools_manager_delegate.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

HeadlessContentBrowserClient::HeadlessContentBrowserClient(
    HeadlessBrowserImpl* browser)
    : browser_(browser) {}

HeadlessContentBrowserClient::~HeadlessContentBrowserClient() {}

content::BrowserMainParts* HeadlessContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams&) {
  std::unique_ptr<HeadlessBrowserMainParts> browser_main_parts =
      base::MakeUnique<HeadlessBrowserMainParts>(browser_);
  browser_->set_browser_main_parts(browser_main_parts.get());
  return browser_main_parts.release();
}

void HeadlessContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    content::WebPreferences* prefs) {
  auto browser_context = HeadlessBrowserContextImpl::From(
      render_view_host->GetProcess()->GetBrowserContext());
  const base::Callback<void(headless::WebPreferences*)>& callback =
      browser_context->options()->override_web_preferences_callback();
  if (callback)
    callback.Run(prefs);
}

content::DevToolsManagerDelegate*
HeadlessContentBrowserClient::GetDevToolsManagerDelegate() {
  return new HeadlessDevToolsManagerDelegate(browser_->GetWeakPtr());
}

std::unique_ptr<base::Value>
HeadlessContentBrowserClient::GetServiceManifestOverlay(
    const std::string& name) {
  if (name != content::kBrowserServiceName)
    return nullptr;
  base::StringPiece manifest_contents =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_HEADLESS_BROWSER_MANIFEST_OVERLAY);
  return base::JSONReader::Read(manifest_contents);
}

}  // namespace headless
