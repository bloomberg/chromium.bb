// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/task.h"
#include "base/win/windows_version.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_registrar.h"

#if defined(OS_LINUX)
#include "base/environment.h"
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#endif  // defined(OS_WIN)

namespace {

#if defined(OS_WIN)
// UpdateShortcutWorker holds all context data needed for update shortcut.
// It schedules a pre-update check to find all shortcuts that needs to be
// updated. If there are such shortcuts, it schedules icon download and
// update them when icons are downloaded. It observes TAB_CLOSING notification
// and cancels all the work when the underlying tab is closing.
class UpdateShortcutWorker : public NotificationObserver {
 public:
  explicit UpdateShortcutWorker(TabContentsWrapper* tab_contents);

  void Run();

 private:
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Downloads icon via TabContents.
  void DownloadIcon();

  // Callback when icon downloaded.
  void OnIconDownloaded(int download_id, bool errored, const SkBitmap& image);

  // Checks if shortcuts exists on desktop, start menu and quick launch.
  void CheckExistingShortcuts();

  // Update shortcut files and icons.
  void UpdateShortcuts();
  void UpdateShortcutsOnFileThread();

  // Callback after shortcuts are updated.
  void OnShortcutsUpdated(bool);

  // Deletes the worker on UI thread where it gets created.
  void DeleteMe();
  void DeleteMeOnUIThread();

  NotificationRegistrar registrar_;

  // Underlying TabContentsWrapper whose shortcuts will be updated.
  TabContentsWrapper* tab_contents_;

  // Icons info from tab_contents_'s web app data.
  web_app::IconInfoList unprocessed_icons_;

  // Cached shortcut data from the tab_contents_.
  ShellIntegration::ShortcutInfo shortcut_info_;

  // Our copy of profile path.
  FilePath profile_path_;

  // File name of shortcut/ico file based on app title.
  FilePath file_name_;

  // Existing shortcuts.
  std::vector<FilePath> shortcut_files_;

  DISALLOW_COPY_AND_ASSIGN(UpdateShortcutWorker);
};

UpdateShortcutWorker::UpdateShortcutWorker(TabContentsWrapper* tab_contents)
    : tab_contents_(tab_contents),
      profile_path_(tab_contents->profile()->GetPath()) {
  web_app::GetShortcutInfoForTab(tab_contents_, &shortcut_info_);
  web_app::GetIconsInfo(tab_contents_->extension_tab_helper()->web_app_info(),
                        &unprocessed_icons_);
  file_name_ = web_app::internals::GetSanitizedFileName(shortcut_info_.title);

  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(&tab_contents_->controller()));
}

void UpdateShortcutWorker::Run() {
  // Starting by downloading app icon.
  DownloadIcon();
}

void UpdateShortcutWorker::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (type == NotificationType::TAB_CLOSING &&
      Source<NavigationController>(source).ptr() ==
        &tab_contents_->controller()) {
    // Underlying tab is closing.
    tab_contents_ = NULL;
  }
}

void UpdateShortcutWorker::DownloadIcon() {
  // FetchIcon must run on UI thread because it relies on TabContents
  // to download the icon.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (tab_contents_ == NULL) {
    DeleteMe();  // We are done if underlying TabContents is gone.
    return;
  }

  if (unprocessed_icons_.empty()) {
    // No app icon. Just use the favicon from TabContents.
    UpdateShortcuts();
    return;
  }

  tab_contents_->tab_contents()->favicon_helper().DownloadImage(
      unprocessed_icons_.back().url,
      std::max(unprocessed_icons_.back().width,
               unprocessed_icons_.back().height),
      history::FAVICON,
      NewCallback(this, &UpdateShortcutWorker::OnIconDownloaded));
  unprocessed_icons_.pop_back();
}

void UpdateShortcutWorker::OnIconDownloaded(int download_id,
                                            bool errored,
                                            const SkBitmap& image) {
  if (tab_contents_ == NULL) {
    DeleteMe();  // We are done if underlying TabContents is gone.
    return;
  }

  if (!errored && !image.isNull()) {
    // Update icon with download image and update shortcut.
    shortcut_info_.favicon = image;
    tab_contents_->extension_tab_helper()->SetAppIcon(image);
    UpdateShortcuts();
  } else {
    // Try the next icon otherwise.
    DownloadIcon();
  }
}

void UpdateShortcutWorker::CheckExistingShortcuts() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Locations to check to shortcut_paths.
  struct {
    bool& use_this_location;
    int location_id;
    const wchar_t* sub_dir;
  } locations[] = {
    {
      shortcut_info_.create_on_desktop,
      chrome::DIR_USER_DESKTOP,
      NULL
    }, {
      shortcut_info_.create_in_applications_menu,
      base::DIR_START_MENU,
      NULL
    }, {
      shortcut_info_.create_in_quick_launch_bar,
      // For Win7, create_in_quick_launch_bar means pinning to taskbar.
      base::DIR_APP_DATA,
      (base::win::GetVersion() >= base::win::VERSION_WIN7) ?
          L"Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar" :
          L"Microsoft\\Internet Explorer\\Quick Launch"
    }
  };

  for (int i = 0; i < arraysize(locations); ++i) {
    locations[i].use_this_location = false;

    FilePath path;
    if (!PathService::Get(locations[i].location_id, &path)) {
      NOTREACHED();
      continue;
    }

    if (locations[i].sub_dir != NULL)
      path = path.Append(locations[i].sub_dir);

    FilePath shortcut_file = path.Append(file_name_).
        ReplaceExtension(FILE_PATH_LITERAL(".lnk"));
    if (file_util::PathExists(shortcut_file)) {
      locations[i].use_this_location = true;
      shortcut_files_.push_back(shortcut_file);
    }
  }
}

void UpdateShortcutWorker::UpdateShortcuts() {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
      &UpdateShortcutWorker::UpdateShortcutsOnFileThread));
}

void UpdateShortcutWorker::UpdateShortcutsOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath web_app_path = web_app::internals::GetWebAppDataDirectory(
      web_app::GetDataDir(profile_path_), shortcut_info_);

  // Ensure web_app_path exists. web_app_path could be missing for a legacy
  // shortcut created by Gears.
  if (!file_util::PathExists(web_app_path) &&
      !file_util::CreateDirectory(web_app_path)) {
    NOTREACHED();
    return;
  }

  FilePath icon_file = web_app_path.Append(file_name_).ReplaceExtension(
      FILE_PATH_LITERAL(".ico"));
  web_app::internals::CheckAndSaveIcon(icon_file, shortcut_info_.favicon);

  // Update existing shortcuts' description, icon and app id.
  CheckExistingShortcuts();
  if (!shortcut_files_.empty()) {
    // Generates app id from web app url and profile path.
    std::wstring app_id = ShellIntegration::GetAppId(
        UTF8ToWide(web_app::GenerateApplicationNameFromURL(shortcut_info_.url)),
        profile_path_);

    // Sanitize description
    if (shortcut_info_.description.length() >= MAX_PATH)
      shortcut_info_.description.resize(MAX_PATH - 1);

    for (size_t i = 0; i < shortcut_files_.size(); ++i) {
      file_util::UpdateShortcutLink(NULL,
          shortcut_files_[i].value().c_str(),
          NULL,
          NULL,
          shortcut_info_.description.c_str(),
          icon_file.value().c_str(),
          0,
          app_id.c_str());
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
      NewRunnableMethod(this, &UpdateShortcutWorker::DeleteMeOnUIThread));
  }
}

void UpdateShortcutWorker::DeleteMeOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delete this;
}
#endif  // defined(OS_WIN)

}  // namespace

#if defined(OS_WIN)
// Allows UpdateShortcutWorker without adding refcounting. UpdateShortcutWorker
// manages its own life time and will delete itself when it's done.
DISABLE_RUNNABLE_METHOD_REFCOUNT(UpdateShortcutWorker);
#endif  // defined(OS_WIN)

namespace web_app {

void GetShortcutInfoForTab(TabContentsWrapper* tab_contents_wrapper,
                           ShellIntegration::ShortcutInfo* info) {
  DCHECK(info);  // Must provide a valid info.
  const TabContents* tab_contents = tab_contents_wrapper->tab_contents();

  const WebApplicationInfo& app_info =
      tab_contents_wrapper->extension_tab_helper()->web_app_info();

  info->url = app_info.app_url.is_empty() ? tab_contents->GetURL() :
                                            app_info.app_url;
  info->title = app_info.title.empty() ?
      (tab_contents->GetTitle().empty() ? UTF8ToUTF16(info->url.spec()) :
                                          tab_contents->GetTitle()) :
      app_info.title;
  info->description = app_info.description;
  info->favicon = tab_contents->GetFavicon();
}

void UpdateShortcutForTabContents(TabContentsWrapper* tab_contents) {
#if defined(OS_WIN)
  // UpdateShortcutWorker will delete itself when it's done.
  UpdateShortcutWorker* worker = new UpdateShortcutWorker(tab_contents);
  worker->Run();
#endif  // defined(OS_WIN)
}

}  // namespace web_app
