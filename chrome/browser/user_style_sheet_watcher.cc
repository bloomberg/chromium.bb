// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_style_sheet_watcher.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace {

// The subdirectory of the profile that contains the style sheet.
const char kStyleSheetDir[] = "User StyleSheets";
// The filename of the stylesheet.
const char kUserStyleSheetFile[] = "Custom.css";

}  // namespace

UserStyleSheetWatcher::UserStyleSheetWatcher(const FilePath& profile_path)
    : profile_path_(profile_path),
      has_loaded_(false) {
  // Listen for when the first render view host is created.  If we load
  // too fast, the first tab won't hear the notification and won't get
  // the user style sheet.
  registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
                 NotificationService::AllSources());
}

void UserStyleSheetWatcher::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  DCHECK(type == NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB);

  if (has_loaded_) {
    NotificationService::current()->Notify(
        NotificationType::USER_STYLE_SHEET_UPDATED,
        Source<UserStyleSheetWatcher>(this),
        NotificationService::NoDetails());
  }

  registrar_.RemoveAll();
}

void UserStyleSheetWatcher::OnFileChanged(const FilePath& path) {
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &UserStyleSheetWatcher::LoadStyleSheet,
                        profile_path_));
}

void UserStyleSheetWatcher::Init() {
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &UserStyleSheetWatcher::LoadStyleSheet,
                        profile_path_));
}

void UserStyleSheetWatcher::LoadStyleSheet(const FilePath& profile_path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  // We keep the user style sheet in a subdir so we can watch for changes
  // to the file.
  FilePath style_sheet_dir = profile_path.AppendASCII(kStyleSheetDir);
  if (!file_util::DirectoryExists(style_sheet_dir)) {
    if (!file_util::CreateDirectory(style_sheet_dir))
      return;
  }
  // Create the file if it doesn't exist.
  FilePath css_file = style_sheet_dir.AppendASCII(kUserStyleSheetFile);
  if (!file_util::PathExists(css_file))
    file_util::WriteFile(css_file, "", 0);

  std::string css;
  bool rv = file_util::ReadFileToString(css_file, &css);
  GURL style_sheet_url;
  if (rv && !css.empty()) {
    std::string css_base64;
    rv = base::Base64Encode(css, &css_base64);
    if (rv) {
      // WebKit knows about data urls, so convert the file to a data url.
      const char kDataUrlPrefix[] = "data:text/css;charset=utf-8;base64,";
      style_sheet_url = GURL(kDataUrlPrefix + css_base64);
    }
  }
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &UserStyleSheetWatcher::SetStyleSheet,
                        style_sheet_url));

  if (!file_watcher_.get()) {
    file_watcher_.reset(new FileWatcher);
    file_watcher_->Watch(profile_path_.AppendASCII(kStyleSheetDir)
                         .AppendASCII(kUserStyleSheetFile), this);
  }
}

void UserStyleSheetWatcher::SetStyleSheet(const GURL& url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  has_loaded_ = true;
  user_style_sheet_ = url;
  NotificationService::current()->Notify(
      NotificationType::USER_STYLE_SHEET_UPDATED,
      Source<UserStyleSheetWatcher>(this),
      NotificationService::NoDetails());
}
