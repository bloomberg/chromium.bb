// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

BookmarkAppInstallManager::BookmarkAppInstallManager() = default;

BookmarkAppInstallManager::~BookmarkAppInstallManager() = default;

bool BookmarkAppInstallManager::CanInstallWebApp(
    content::WebContents* web_contents) {
  return extensions::TabHelper::FromWebContents(web_contents)
      ->CanCreateBookmarkApp();
}

void BookmarkAppInstallManager::InstallWebApp(
    content::WebContents* web_contents,
    bool force_shortcut_app,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  // Ignore dialog_callback for legacy installations.
  // BookmarkAppHelper directly uses chrome::ShowPWAInstallDialog from UI
  // (which is a layering violation). TODO(loyso): Unify it. crbug.com/915043.
  extensions::TabHelper::FromWebContents(web_contents)
      ->CreateHostedAppFromWebContents(
          force_shortcut_app,
          base::BindOnce(
              [](OnceInstallCallback callback, const ExtensionId& app_id,
                 bool success) {
                std::move(callback).Run(
                    app_id,
                    success ? web_app::InstallResultCode::kSuccess
                            : web_app::InstallResultCode::kFailedUnknownReason);
              },
              std::move(callback)));
}

}  // namespace extensions
