// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

// Note that md_settings.html contains a <script> tag which imports a script of
// the following name. These names must be kept in sync.
const char kLocalizedStringsFile[] = "strings.js";

}  // namespace

namespace settings {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "a11yPageTitle", IDS_MD_SETTINGS_ACCESSIBILITY_PAGE_TITLE);
  html_source->AddLocalizedString(
      "downloadsPageTitle", IDS_MD_SETTINGS_DOWNLOADS_PAGE_TITLE);

  html_source->SetJsonPath(kLocalizedStringsFile);
}

}  // namespace settings
