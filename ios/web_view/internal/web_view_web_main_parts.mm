// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_main_parts.h"

#include "base/base_paths.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_delegate.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewWebMainParts::WebViewWebMainParts(id<CWVDelegate> delegate) {
  delegate_ = delegate;
}

WebViewWebMainParts::~WebViewWebMainParts() = default;

void WebViewWebMainParts::PreMainMessageLoopRun() {
  // Initialize resources.
  l10n_util::OverrideLocaleWithCocoaLocale();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      std::string(), nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  base::FilePath pak_file;
  PathService::Get(base::DIR_MODULE, &pak_file);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("web_view_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_NONE);

  browser_state_ = base::MakeUnique<WebViewBrowserState>(false);
  off_the_record_browser_state_ = base::MakeUnique<WebViewBrowserState>(true);

  // Initialize translate.
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  // TODO(crbug.com/679895): See if we need the system request context here.
  download_manager->set_request_context(browser_state_->GetRequestContext());
  // TODO(crbug.com/679895): Bring up application locale correctly.
  download_manager->set_application_locale(l10n_util::GetLocaleOverride());
  download_manager->language_list()->SetResourceRequestsAllowed(true);
}

}  // namespace ios_web_view
