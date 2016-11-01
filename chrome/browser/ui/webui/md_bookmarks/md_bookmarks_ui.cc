// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_bookmarks/md_bookmarks_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

content::WebUIDataSource* CreateMdBookmarksUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBookmarksHost);

  // Localized strings (alphabetical order).
  source->AddLocalizedString("title", IDS_BOOKMARK_MANAGER_TITLE);

  source->SetDefaultResource(IDR_MD_BOOKMARKS_BOOKMARKS_HTML);
  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

MdBookmarksUI::MdBookmarksUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://bookmarks/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreateMdBookmarksUIHTMLSource(profile));
}

// static
bool MdBookmarksUI::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kMaterialDesignBookmarks);
}
