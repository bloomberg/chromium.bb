// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/update_shortcut_worker_win.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_win.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/icon_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationController;
using content::WebContents;

namespace web_app {

UpdateShortcutWorker::UpdateShortcutWorker(WebContents* web_contents)
    : web_contents_(web_contents),
      profile_path_(Profile::FromBrowserContext(
          web_contents->GetBrowserContext())->GetPath()) {
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(web_contents);
  web_app::GetShortcutInfoForTab(web_contents_, &shortcut_info_);
  web_app::GetIconsInfo(extensions_tab_helper->web_app_info(),
                        &unprocessed_icons_);
  file_name_ = web_app::internals::GetSanitizedFileName(shortcut_info_.title);

  registrar_.Add(
      this,
      chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(&web_contents->GetController()));
}

void UpdateShortcutWorker::Run() {
  // Starting by downloading app icon.
  DownloadIcon();
}

void UpdateShortcutWorker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_TAB_CLOSING &&
      content::Source<NavigationController>(source).ptr() ==
        &web_contents_->GetController()) {
    // Underlying tab is closing.
    web_contents_ = NULL;
  }
}

void UpdateShortcutWorker::DownloadIcon() {
  // FetchIcon must run on UI thread because it relies on WebContents
  // to download the icon.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (web_contents_ == NULL) {
    DeleteMe();  // We are done if underlying WebContents is gone.
    return;
  }

  if (unprocessed_icons_.empty()) {
    // No app icon. Just use the favicon from WebContents.
    UpdateShortcuts();
    return;
  }

  int preferred_size = std::max(unprocessed_icons_.back().width,
                                unprocessed_icons_.back().height);
  web_contents_->DownloadImage(
      unprocessed_icons_.back().url,
      true,  // favicon
      0,  // no maximum size
      base::Bind(&UpdateShortcutWorker::DidDownloadFavicon,
                 base::Unretained(this),
                 preferred_size));
  unprocessed_icons_.pop_back();
}

void UpdateShortcutWorker::DidDownloadFavicon(
    int requested_size,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_sizes) {
  std::vector<int> requested_sizes_in_pixel;
  requested_sizes_in_pixel.push_back(requested_size);

  std::vector<size_t> closest_indices;
  SelectFaviconFrameIndices(
      original_sizes, requested_sizes_in_pixel, &closest_indices, NULL);

  SkBitmap bitmap;
  if (!bitmaps.empty()) {
     size_t closest_index = closest_indices[0];
     bitmap = bitmaps[closest_index];
  }

  if (!bitmap.isNull()) {
    // Update icon with download image and update shortcut.
    shortcut_info_.favicon.Add(gfx::Image::CreateFrom1xBitmap(bitmap));
    extensions::TabHelper* extensions_tab_helper =
        extensions::TabHelper::FromWebContents(web_contents_);
    extensions_tab_helper->SetAppIcon(bitmap);
    UpdateShortcuts();
  } else {
    // Try the next icon otherwise.
    DownloadIcon();
  }
}

void UpdateShortcutWorker::CheckExistingShortcuts() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // Locations to check to shortcut_paths.
  struct {
    int location_id;
    const wchar_t* sub_dir;
  } locations[] = {
    {
      base::DIR_USER_DESKTOP,
      NULL
    }, {
      base::DIR_START_MENU,
      NULL
    }, {
      // For Win7, create_in_quick_launch_bar means pinning to taskbar.
      base::DIR_APP_DATA,
      (base::win::GetVersion() >= base::win::VERSION_WIN7) ?
          L"Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar" :
          L"Microsoft\\Internet Explorer\\Quick Launch"
    }
  };

  for (int i = 0; i < arraysize(locations); ++i) {
    base::FilePath path;
    if (!PathService::Get(locations[i].location_id, &path)) {
      NOTREACHED();
      continue;
    }

    if (locations[i].sub_dir != NULL)
      path = path.Append(locations[i].sub_dir);

    base::FilePath shortcut_file = path.Append(file_name_).
        ReplaceExtension(FILE_PATH_LITERAL(".lnk"));
    if (base::PathExists(shortcut_file)) {
      shortcut_files_.push_back(shortcut_file);
    }
  }
}

void UpdateShortcutWorker::UpdateShortcuts() {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&UpdateShortcutWorker::UpdateShortcutsOnFileThread,
                 base::Unretained(this)));
}

void UpdateShortcutWorker::UpdateShortcutsOnFileThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  base::FilePath web_app_path = web_app::GetWebAppDataDirectory(
      profile_path_, shortcut_info_.extension_id, shortcut_info_.url);

  // Ensure web_app_path exists. web_app_path could be missing for a legacy
  // shortcut created by Gears.
  if (!base::PathExists(web_app_path) &&
      !base::CreateDirectory(web_app_path)) {
    NOTREACHED();
    return;
  }

  base::FilePath icon_file =
      web_app::internals::GetIconFilePath(web_app_path, shortcut_info_.title);
  web_app::internals::CheckAndSaveIcon(icon_file, shortcut_info_.favicon);

  // Update existing shortcuts' description, icon and app id.
  CheckExistingShortcuts();
  if (!shortcut_files_.empty()) {
    // Generates app id from web app url and profile path.
    base::string16 app_id = ShellIntegration::GetAppModelIdForProfile(
        base::UTF8ToWide(
            web_app::GenerateApplicationNameFromURL(shortcut_info_.url)),
        profile_path_);

    // Sanitize description
    if (shortcut_info_.description.length() >= MAX_PATH)
      shortcut_info_.description.resize(MAX_PATH - 1);

    for (size_t i = 0; i < shortcut_files_.size(); ++i) {
      base::win::ShortcutProperties shortcut_properties;
      shortcut_properties.set_target(shortcut_files_[i]);
      shortcut_properties.set_description(shortcut_info_.description);
      shortcut_properties.set_icon(icon_file, 0);
      shortcut_properties.set_app_id(app_id);
      base::win::CreateOrUpdateShortcutLink(
          shortcut_files_[i], shortcut_properties,
          base::win::SHORTCUT_UPDATE_EXISTING);
    }
  }

  OnShortcutsUpdated(true);
}

void UpdateShortcutWorker::OnShortcutsUpdated(bool) {
  DeleteMe();  // We are done.
}

void UpdateShortcutWorker::DeleteMe() {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DeleteMeOnUIThread();
  } else {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&UpdateShortcutWorker::DeleteMeOnUIThread,
                 base::Unretained(this)));
  }
}

void UpdateShortcutWorker::DeleteMeOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delete this;
}

}  // namespace web_app
