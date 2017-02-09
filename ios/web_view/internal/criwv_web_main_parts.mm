// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/criwv_web_main_parts.h"

#include "base/base_paths.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "ios/web_view/internal/criwv_browser_state.h"
#import "ios/web_view/public/criwv_delegate.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

namespace ios_web_view {

CRIWVWebMainParts::CRIWVWebMainParts(id<CRIWVDelegate> delegate) {
  delegate_ = delegate;
}

CRIWVWebMainParts::~CRIWVWebMainParts() {}

void CRIWVWebMainParts::PreMainMessageLoopRun() {
  // Initialize resources.
  l10n_util::OverrideLocaleWithCocoaLocale();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      std::string(), nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  base::FilePath pak_file;
  PathService::Get(base::DIR_MODULE, &pak_file);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("web_view_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_NONE);

  browser_state_ = base::MakeUnique<CRIWVBrowserState>(false);
  off_the_record_browser_state_ = base::MakeUnique<CRIWVBrowserState>(true);

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
