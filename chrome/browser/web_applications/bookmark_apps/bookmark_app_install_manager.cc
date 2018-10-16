// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

BookmarkAppInstallManager::BookmarkAppInstallManager() = default;

BookmarkAppInstallManager::~BookmarkAppInstallManager() = default;

bool BookmarkAppInstallManager::CanInstallWebApp(
    const content::WebContents* web_contents) {
  return extensions::TabHelper::FromWebContents(web_contents)
      ->CanCreateBookmarkApp();
}

void BookmarkAppInstallManager::InstallWebApp(
    content::WebContents* web_contents,
    bool force_shortcut_app) {
  extensions::TabHelper::FromWebContents(web_contents)
      ->CreateHostedAppFromWebContents(force_shortcut_app);
}

}  // namespace extensions
