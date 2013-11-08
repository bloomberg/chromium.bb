// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/action_choice_dialog.h"

#include "ash/shell.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/url_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/screen.h"

namespace file_manager {
namespace util {
namespace {

// Finds a browser instance showing the target URL. Returns NULL if not
// found.
Browser* FindBrowserForUrl(GURL target_url) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int idx = 0; idx < tab_strip->count(); idx++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(idx);
      const GURL& url = web_contents->GetLastCommittedURL();
      if (url == target_url)
        return browser;
    }
  }
  return NULL;
}

}  // namespace

void OpenActionChoiceDialog(const base::FilePath& file_path,
                            bool advanced_mode) {
  const int kDialogWidth = 394;
  // TODO(dgozman): remove 50, which is a title height once popup window
  // will have no title.
  const int kDialogHeight = 316 + 50;

  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();

  base::FilePath virtual_path;
  if (!ConvertAbsoluteFilePathToRelativeFileSystemPath(
          profile, kFileManagerAppId, file_path, &virtual_path))
    return;
  GURL dialog_url = GetActionChoiceUrl(virtual_path, advanced_mode);

  const gfx::Size screen = ash::Shell::GetScreen()->GetPrimaryDisplay().size();
  const gfx::Rect bounds((screen.width() - kDialogWidth) / 2,
                         (screen.height() - kDialogHeight) / 2,
                         kDialogWidth,
                         kDialogHeight);

  Browser* browser = FindBrowserForUrl(dialog_url);

  if (browser) {
    browser->window()->Show();
    return;
  }

  ExtensionService* service = extensions::ExtensionSystem::Get(profile)->
      extension_service();
  if (!service)
    return;

  const extensions::Extension* extension =
      service->GetExtensionById(kFileManagerAppId, false);
  if (!extension)
    return;

  AppLaunchParams params(profile, extension, extensions::LAUNCH_WINDOW,
                         NEW_FOREGROUND_TAB);
  params.override_url = dialog_url;
  params.override_bounds = bounds;
  OpenApplication(params);
}

}  // namespace util
}  // namespace file_manager
