// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#if defined(OS_WIN)
#include <ShellAPI.h>
#endif  // defined(OS_WIN)

#include "base/file_util.h"
#include "base/md5.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/thread.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "webkit/glue/dom_operations.h"

#if defined(OS_WIN)
#include "app/gfx/icon_util.h"
#include "base/win_util.h"
#endif  // defined(OS_WIN)

namespace {

const FilePath::CharType kIconChecksumFileExt[] = FILE_PATH_LITERAL(".ico.md5");

// Returns true if |ch| is in visible ASCII range and not one of
// "/ \ : * ? " < > | ; ,".
bool IsValidFilePathChar(char16 c) {
  if (c < 32)
    return false;

  switch (c) {
    case '/':
    case '\\':
    case ':':
    case '*':
    case '?':
    case '"':
    case '<':
    case '>':
    case '|':
    case ';':
    case ',':
      return false;
  };

  return true;
}

// Returns sanitized name that could be used as a file name
FilePath GetSanitizedFileName(const string16& name) {
  string16 file_name;

  for (size_t i = 0; i < name.length(); ++i) {
    char16 c = name[i];
    if (!IsValidFilePathChar(c))
      c = '_';

    file_name += c;
  }

#if defined(OS_WIN)
  return FilePath(file_name);
#elif defined(OS_POSIX)
  return FilePath(UTF16ToUTF8(file_name));
#endif
}

// Returns relative directory of given web app url.
FilePath GetWebAppDir(const GURL& url) {
  FilePath::StringType host;
  FilePath::StringType scheme_port;

#if defined(OS_WIN)
  host = UTF8ToWide(url.host());
  scheme_port = (url.has_scheme() ? UTF8ToWide(url.scheme()) : L"http") +
      FILE_PATH_LITERAL("_") +
      (url.has_port() ? UTF8ToWide(url.port()) : L"80");
#elif defined(OS_POSIX)
  host = url.host();
  scheme_port = url.scheme() + FILE_PATH_LITERAL("_") + url.port();
#endif

  return FilePath(host).Append(scheme_port);
}

// Returns data directory for given web app url
FilePath GetWebAppDataDirectory(const FilePath& root_dir,
                                const GURL& url) {
  return root_dir.Append(GetWebAppDir(url));
}

// Predicator for sorting images from largest to smallest.
bool IconPrecedes(
    const webkit_glue::WebApplicationInfo::IconInfo& left,
    const webkit_glue::WebApplicationInfo::IconInfo& right) {
  return left.width < right.width;
}

// Calculates image checksum using MD5.
void GetImageCheckSum(const SkBitmap& image, MD5Digest* digest) {
  DCHECK(digest);

  SkAutoLockPixels image_lock(image);
  MD5Sum(image.getPixels(), image.getSize(), digest);
}

#if defined(OS_WIN)
// Saves |image| as an |icon_file| with the checksum.
bool SaveIconWithCheckSum(const FilePath& icon_file, const SkBitmap& image) {
  if (!IconUtil::CreateIconFileFromSkBitmap(image, icon_file.value()))
    return false;

  MD5Digest digest;
  GetImageCheckSum(image, &digest);

  FilePath cheksum_file(icon_file.ReplaceExtension(kIconChecksumFileExt));
  return file_util::WriteFile(cheksum_file,
                              reinterpret_cast<const char*>(&digest),
                              sizeof(digest)) == sizeof(digest);
}

// Returns true if |icon_file| is missing or different from |image|.
bool ShouldUpdateIcon(const FilePath& icon_file, const SkBitmap& image) {
  FilePath checksum_file(icon_file.ReplaceExtension(kIconChecksumFileExt));

  // Returns true if icon_file or checksum file is missing.
  if (!file_util::PathExists(icon_file) ||
      !file_util::PathExists(checksum_file))
    return true;

  MD5Digest persisted_image_checksum;
  if (sizeof(persisted_image_checksum) != file_util::ReadFile(checksum_file,
                      reinterpret_cast<char*>(&persisted_image_checksum),
                      sizeof(persisted_image_checksum)))
    return true;

  MD5Digest downloaded_image_checksum;
  GetImageCheckSum(image, &downloaded_image_checksum);

  // Update icon if checksums are not equal.
  return memcmp(&persisted_image_checksum, &downloaded_image_checksum,
                sizeof(MD5Digest)) != 0;
}

// Saves |image| to |icon_file| if the file is outdated and refresh shell's
// icon cache to ensure correct icon is displayed. Returns true if icon_file
// is up to date or successfully updated.
bool CheckAndSaveIcon(const FilePath& icon_file, const SkBitmap& image) {
  if (ShouldUpdateIcon(icon_file, image)) {
    if (SaveIconWithCheckSum(icon_file, image)) {
      // Refresh shell's icon cache. This call is quite disruptive as user would
      // see explorer rebuilding the icon cache. It would be great that we find
      // a better way to achieve this.
      SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT,
                     NULL, NULL);
    } else {
      return false;
    }
  }

  return true;
}

#endif  // defined(OS_WIN)

// Represents a task that creates web application shortcut. This runs on
// file thread and schedules the callback (if any) on the calling thread
// when finished (either success or failure).
class CreateShortcutTask : public Task {
 public:
  CreateShortcutTask(const FilePath& profile_path,
                     const ShellIntegration::ShortcutInfo& shortcut_info,
                     web_app::CreateShortcutCallback* callback);

 private:
  class CreateShortcutCallbackTask : public Task {
   public:
    CreateShortcutCallbackTask(web_app::CreateShortcutCallback* callback,
        bool success)
        : callback_(callback),
          success_(success) {
    }

    // Overridden from Task:
    virtual void Run() {
      callback_->Run(success_);
    }

   private:
    web_app::CreateShortcutCallback* callback_;
    bool success_;
  };

  // Overridden from Task:
  virtual void Run();

  // Returns true if shortcut is created successfully.
  bool CreateShortcut();

  // Path to store persisted data for web app.
  FilePath web_app_path_;

  // Out copy of profile path.
  FilePath profile_path_;

  // Our copy of short cut data.
  ShellIntegration::ShortcutInfo shortcut_info_;

  // Callback when task is finished.
  web_app::CreateShortcutCallback* callback_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(CreateShortcutTask);
};

CreateShortcutTask::CreateShortcutTask(
    const FilePath& profile_path,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    web_app::CreateShortcutCallback* callback)
    : web_app_path_(GetWebAppDataDirectory(web_app::GetDataDir(profile_path),
                                           shortcut_info.url)),
      profile_path_(profile_path),
      shortcut_info_(shortcut_info),
      callback_(callback),
      message_loop_(MessageLoop::current()) {
  DCHECK(message_loop_ != NULL);
}

void CreateShortcutTask::Run() {
  bool success = CreateShortcut();

  if (callback_ != NULL)
    message_loop_->PostTask(FROM_HERE,
      new CreateShortcutCallbackTask(callback_, success));
}

bool CreateShortcutTask::CreateShortcut() {
#if defined(OS_LINUX)
  ShellIntegration::CreateDesktopShortcut(shortcut_info_);
  return true;  // assuming always success.
#elif defined(OS_WIN)
  // Shortcut paths under which to create shortcuts.
  std::vector<FilePath> shortcut_paths;

  // Locations to add to shortcut_paths.
  struct {
    const bool& use_this_location;
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
      // For Win7, create_in_quick_launch_bar means pinning to taskbar. Use
      // base::PATH_START as a flag for this case.
      (win_util::GetWinVersion() >= win_util::WINVERSION_WIN7) ?
          base::PATH_START : base::DIR_APP_DATA,
      (win_util::GetWinVersion() >= win_util::WINVERSION_WIN7) ?
          NULL : L"Microsoft\\Internet Explorer\\Quick Launch"
    }
  };

  // Populate shortcut_paths.
  for (int i = 0; i < arraysize(locations); ++i) {
    if (locations[i].use_this_location) {
      FilePath path;

      // Skip the Win7 case.
      if (locations[i].location_id == base::PATH_START)
        continue;

      if (!PathService::Get(locations[i].location_id, &path)) {
        NOTREACHED();
        return false;
      }

      if (locations[i].sub_dir != NULL)
        path = path.Append(locations[i].sub_dir);

      shortcut_paths.push_back(path);
    }
  }

  bool pin_to_taskbar =
      shortcut_info_.create_in_quick_launch_bar &&
      (win_util::GetWinVersion() >= win_util::WINVERSION_WIN7);

  // For Win7's pinning support, any shortcut could be used. So we only create
  // the shortcut file when there is no shortcut file will be created. That is,
  // user only selects "Pin to taskbar".
  if (pin_to_taskbar && shortcut_paths.empty()) {
    // Creates the shortcut in web_app_path_ in this case.
    shortcut_paths.push_back(web_app_path_);
  }

  if (shortcut_paths.empty()) {
    NOTREACHED();
    return false;
  }

  // Ensure web_app_path_ exists.
  if (!file_util::PathExists(web_app_path_) &&
      !file_util::CreateDirectory(web_app_path_)) {
    NOTREACHED();
    return false;
  }

  // Generates file name to use with persisted ico and shortcut file.
  FilePath file_name = GetSanitizedFileName(shortcut_info_.title);

  // Creates an ico file to use with shortcut.
  FilePath icon_file = web_app_path_.Append(file_name).ReplaceExtension(
      FILE_PATH_LITERAL(".ico"));
  if (!CheckAndSaveIcon(icon_file, shortcut_info_.favicon)) {
    NOTREACHED();
    return false;
  }

  std::wstring chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return false;
  }

  // Working directory.
  std::wstring chrome_folder = file_util::GetDirectoryFromPath(chrome_exe);

  // Gets the command line switches.
  std::string switches;
  if (CPB_GetCommandLineArgumentsCommon(shortcut_info_.url.spec().c_str(),
      &switches) != CPERR_SUCCESS) {
    NOTREACHED();
    return false;
  }
  std::wstring wide_switchs(UTF8ToWide(switches));

  // Generates app id from web app url and profile path.
  std::wstring app_id = ShellIntegration::GetAppId(
      web_app::GenerateApplicationNameFromURL(shortcut_info_.url).c_str(),
      profile_path_);

  bool success = true;
  for (size_t i = 0; i < shortcut_paths.size(); ++i) {
    FilePath shortcut_file = shortcut_paths[i].Append(file_name).
        ReplaceExtension(FILE_PATH_LITERAL(".lnk"));
    success &= file_util::CreateShortcutLink(chrome_exe.c_str(),
        shortcut_file.value().c_str(),
        chrome_folder.c_str(),
        wide_switchs.c_str(),
        shortcut_info_.description.c_str(),
        icon_file.value().c_str(),
        0,
        app_id.c_str());
  }

  if (success && pin_to_taskbar) {
    // Any shortcut would work for the pinning. We use the first one.
    FilePath shortcut_file = shortcut_paths[0].Append(file_name).
        ReplaceExtension(FILE_PATH_LITERAL(".lnk"));
    success &= file_util::TaskbarPinShortcutLink(shortcut_file.value().c_str());
  }

  return success;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

#if defined(OS_WIN)
// UpdateShortcutWorker holds all context data needed for update shortcut.
// It schedules a pre-update check to find all shortcuts that needs to be
// updated. If there are such shortcuts, it schedules icon download and
// update them when icons are downloaded. It observes TAB_CLOSING notification
// and cancels all the work when the underlying tab is closing.
class UpdateShortcutWorker : public NotificationObserver {
 public:
  explicit UpdateShortcutWorker(TabContents* tab_contents);

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

  // Underlying TabContents whose shortcuts will be updated.
  TabContents* tab_contents_;

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

UpdateShortcutWorker::UpdateShortcutWorker(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      profile_path_(tab_contents->profile()->GetPath()) {
  web_app::GetShortcutInfoForTab(tab_contents_, &shortcut_info_);
  web_app::GetIconsInfo(tab_contents_->web_app_info(), &unprocessed_icons_);
  file_name_ = GetSanitizedFileName(shortcut_info_.title);

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
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (tab_contents_ == NULL) {
    DeleteMe();  // We are done if underlying TabContents is gone.
    return;
  }

  if (unprocessed_icons_.empty()) {
    // No app icon. Just use the favicon from TabContents.
    UpdateShortcuts();
    return;
  }

  tab_contents_->fav_icon_helper().DownloadImage(
      unprocessed_icons_.back().url,
      std::max(unprocessed_icons_.back().width,
               unprocessed_icons_.back().height),
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
    UpdateShortcuts();
  } else {
    // Try the next icon otherwise.
    DownloadIcon();
  }
}

void UpdateShortcutWorker::CheckExistingShortcuts() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

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
      (win_util::GetWinVersion() >= win_util::WINVERSION_WIN7) ?
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
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
      &UpdateShortcutWorker::UpdateShortcutsOnFileThread));
}

void UpdateShortcutWorker::UpdateShortcutsOnFileThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  FilePath web_app_path = GetWebAppDataDirectory(
      web_app::GetDataDir(profile_path_), shortcut_info_.url);
  FilePath icon_file = web_app_path.Append(file_name_).ReplaceExtension(
      FILE_PATH_LITERAL(".ico"));
  CheckAndSaveIcon(icon_file, shortcut_info_.favicon);

  // Update existing shortcuts' description, icon and app id.
  CheckExistingShortcuts();
  if (!shortcut_files_.empty()) {
    // Generates app id from web app url and profile path.
    std::wstring app_id = ShellIntegration::GetAppId(
        web_app::GenerateApplicationNameFromURL(shortcut_info_.url).c_str(),
        profile_path_);

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
  if (ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    DeleteMeOnUIThread();
  } else {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &UpdateShortcutWorker::DeleteMeOnUIThread));
  }
}

void UpdateShortcutWorker::DeleteMeOnUIThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  delete this;
}
#endif  // defined(OS_WIN)

};  // namespace

#if defined(OS_WIN)
// Allows UpdateShortcutWorker without adding refcounting. UpdateShortcutWorker
// manages its own life time and will delete itself when it's done.
template <>
struct RunnableMethodTraits<UpdateShortcutWorker> {
  void RetainCallee(UpdateShortcutWorker* worker) {}
  void ReleaseCallee(UpdateShortcutWorker* worker) {}
};
#endif  // defined(OS_WIN)

namespace web_app {

std::wstring GenerateApplicationNameFromURL(const GURL& url) {
  std::string t;
  t.append(url.host());
  t.append("_");
  t.append(url.path());
  return UTF8ToWide(t);
}

void CreateShortcut(
    const FilePath& data_dir,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    CreateShortcutCallback* callback) {
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      new CreateShortcutTask(data_dir, shortcut_info, callback));
}

bool IsValidUrl(const GURL& url) {
  static const char* const kValidUrlSchemes[] = {
      chrome::kFileScheme,
      chrome::kFtpScheme,
      chrome::kHttpScheme,
      chrome::kHttpsScheme,
  };

  for (size_t i = 0; i < arraysize(kValidUrlSchemes); ++i) {
    if (url.SchemeIs(kValidUrlSchemes[i]))
      return true;
  }

  return false;
}

FilePath GetDataDir(const FilePath& profile_path) {
  return profile_path.Append(chrome::kWebAppDirname);
}

void GetIconsInfo(const webkit_glue::WebApplicationInfo& app_info,
                  IconInfoList* icons) {
  DCHECK(icons);

  icons->clear();
  for (size_t i = 0; i < app_info.icons.size(); ++i) {
    // We only take square shaped icons (i.e. width == height).
    if (app_info.icons[i].width == app_info.icons[i].height) {
      icons->push_back(app_info.icons[i]);
    }
  }

  std::sort(icons->begin(), icons->end(), &IconPrecedes);
}

void GetShortcutInfoForTab(TabContents* tab_contents,
                           ShellIntegration::ShortcutInfo* info) {
  DCHECK(info);  // Must provide a valid info.

  const webkit_glue::WebApplicationInfo& app_info =
      tab_contents->web_app_info();

  info->url = app_info.app_url.is_empty() ? tab_contents->GetURL() :
                                            app_info.app_url;
  info->title = app_info.title.empty() ?
      (tab_contents->GetTitle().empty() ? UTF8ToUTF16(info->url.spec()) :
                                          tab_contents->GetTitle()) :
      app_info.title;
  info->description = app_info.description;
  info->favicon = tab_contents->GetFavIcon();
}

void UpdateShortcutForTabContents(TabContents* tab_contents) {
#if defined(OS_WIN)
  // UpdateShortcutWorker will delete itself when it's done.
  UpdateShortcutWorker* worker = new UpdateShortcutWorker(tab_contents);
  worker->Run();
#endif  // defined(OS_WIN)
}

};  // namespace web_app
