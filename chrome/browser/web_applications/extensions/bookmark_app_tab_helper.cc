// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_tab_helper.h"

#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

BookmarkAppTabHelper::BookmarkAppTabHelper(content::WebContents* web_contents)
    : WebAppTabHelperBase(web_contents) {}

BookmarkAppTabHelper::~BookmarkAppTabHelper() = default;

// static
BookmarkAppTabHelper* BookmarkAppTabHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  // Do nothing if already exists.
  if (FromWebContents(web_contents))
    return nullptr;

  auto tab_helper = std::make_unique<BookmarkAppTabHelper>(web_contents);
  BookmarkAppTabHelper* result = tab_helper.get();
  web_contents->SetUserData(UserDataKey(), std::move(tab_helper));
  return result;
}

web_app::WebAppTabHelperBase* BookmarkAppTabHelper::CloneForWebContents(
    content::WebContents* web_contents) const {
  BookmarkAppTabHelper* new_tab_helper =
      BookmarkAppTabHelper::CreateForWebContents(web_contents);
  return new_tab_helper;
}

web_app::AppId BookmarkAppTabHelper::GetAppId(const GURL& url) {
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();

  const Extension* extension = nullptr;

  if (base::FeatureList::IsEnabled(::features::kDesktopPWAWindowing))
    extension = util::GetInstalledPwaForUrl(browser_context, url);

  if (!extension) {
    // Check if there is a shortcut app for this |url|.
    extension = GetInstalledShortcutForUrl(browser_context, url);
  }

  return extension ? extension->id() : web_app::AppId();
}

bool BookmarkAppTabHelper::IsInAppWindow() const {
  return util::IsWebContentsInAppWindow(web_contents());
}

}  // namespace extensions
