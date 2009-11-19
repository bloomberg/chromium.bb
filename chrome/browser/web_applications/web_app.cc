// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/thread.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/url_constants.h"

#if defined(OS_WIN)
#include "app/gfx/icon_util.h"
#include "base/win_util.h"
#endif  // defined(OS_WIN)

namespace {

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

// Represents a task that creates web application shortcut. This runs on
// file thread and schedules the callback (if any) on the calling thread
// when finished (either success or failure).
class CreateShortcutTask : public Task {
 public:
  CreateShortcutTask(const FilePath& root_dir,
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

  // Path to store persisted data for web_app.
  FilePath web_app_path_;

  // Our copy of short cut data.
  ShellIntegration::ShortcutInfo shortcut_info_;

  // Callback when task is finished.
  web_app::CreateShortcutCallback* callback_;
  MessageLoop* message_loop_;
};

CreateShortcutTask::CreateShortcutTask(
    const FilePath& root_dir,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    web_app::CreateShortcutCallback* callback)
    : web_app_path_(GetWebAppDataDirectory(root_dir, shortcut_info.url)),
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
      // For Win7, create_in_quick_launch_bar means pining to taskbar. Use
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
  if (!IconUtil::CreateIconFileFromSkBitmap(shortcut_info_.favicon,
                                            icon_file.value())) {
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
        web_app::GenerateApplicationNameFromURL(shortcut_info_.url).c_str());
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

};  // namespace

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
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
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

};  // namespace web_app
