// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_main_parts.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#include "ios/web_view/internal/translate/web_view_translate_service.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewWebMainParts::WebViewWebMainParts() {}

WebViewWebMainParts::~WebViewWebMainParts() = default;

void WebViewWebMainParts::PreMainMessageLoopStart() {
  l10n_util::OverrideLocaleWithCocoaLocale();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      std::string(), nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

  base::FilePath pak_file;
  PathService::Get(base::DIR_MODULE, &pak_file);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("web_view_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_NONE);
}

void WebViewWebMainParts::PreCreateThreads() {
  // Initialize local state.
  PrefService* local_state = ApplicationContext::GetInstance()->GetLocalState();
  DCHECK(local_state);

  ApplicationContext::GetInstance()->PreCreateThreads();
}

void WebViewWebMainParts::PreMainMessageLoopRun() {
  WebViewTranslateService::GetInstance()->Initialize();

  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
      /*schemes=*/nullptr, 0);
}

void WebViewWebMainParts::PostMainMessageLoopRun() {
  WebViewTranslateService::GetInstance()->Shutdown();
  ApplicationContext::GetInstance()->SaveState();
  [CWVWebViewConfiguration shutDown];
}

void WebViewWebMainParts::PostDestroyThreads() {
  ApplicationContext::GetInstance()->PostDestroyThreads();
}

}  // namespace ios_web_view
