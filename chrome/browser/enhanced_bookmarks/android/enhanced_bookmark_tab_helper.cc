// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enhanced_bookmarks/android/enhanced_bookmark_tab_helper.h"

#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enhanced_bookmarks/android/bookmark_image_service_android.h"
#include "chrome/browser/enhanced_bookmarks/android/bookmark_image_service_factory.h"
#include "chrome/browser/profiles/profile.h"

using enhanced_bookmarks::BookmarkImageServiceAndroid;
using enhanced_bookmarks::BookmarkImageServiceFactory;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(EnhancedBookmarkTabHelper);

void EnhancedBookmarkTabHelper::DocumentOnLoadCompletedInMainFrame() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  if (profile->IsOffTheRecord())
    return;

  bool is_enhanced_bookmarks_enabled =
      IsEnhancedBookmarksEnabled(profile->GetPrefs());
  if (!is_enhanced_bookmarks_enabled)
    return;

  BookmarkImageServiceAndroid* storage =
      static_cast<BookmarkImageServiceAndroid*>(
          BookmarkImageServiceFactory::GetForBrowserContext(profile));
  storage->FinishSuccessfulPageLoadForTab(web_contents(),
                                          is_enhanced_bookmarks_enabled);
}

EnhancedBookmarkTabHelper::EnhancedBookmarkTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {
}
