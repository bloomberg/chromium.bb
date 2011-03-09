// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_style_sheet_watcher.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace {

// The subdirectory of the profile that contains the style sheet.
const char kStyleSheetDir[] = "User StyleSheets";
// The filename of the stylesheet.
const char kUserStyleSheetFile[] = "Custom.css";

}  // namespace

// UserStyleSheetLoader is responsible for loading  the user style sheet on the
// file thread and sends a notification when the style sheet is loaded. It is
// a helper to UserStyleSheetWatcher. The reference graph is as follows:
//
// .-----------------------.    owns    .-----------------.
// | UserStyleSheetWatcher |----------->| FilePathWatcher |
// '-----------------------'            '-----------------'
//             |                                 |
//             V                                 |
//  .----------------------.                     |
//  | UserStyleSheetLoader |<--------------------'
//  '----------------------'
//
// FilePathWatcher's reference to UserStyleSheetLoader is used for delivering
// the change notifications. Since they happen asynchronously,
// UserStyleSheetWatcher and its FilePathWatcher may be destroyed while a
// callback to UserStyleSheetLoader is in progress, in which case the
// UserStyleSheetLoader object outlives the watchers.
class UserStyleSheetLoader : public FilePathWatcher::Delegate {
 public:
  UserStyleSheetLoader();
  virtual ~UserStyleSheetLoader() {}

  GURL user_style_sheet() const {
    return user_style_sheet_;
  }

  // Load the user style sheet on the file thread and convert it to a
  // base64 URL.  Posts the base64 URL back to the UI thread.
  void LoadStyleSheet(const FilePath& style_sheet_file);

  // Send out a notification if the stylesheet has already been loaded.
  void NotifyLoaded();

  // FilePathWatcher::Delegate interface
  virtual void OnFilePathChanged(const FilePath& path);

 private:
  // Called on the UI thread after the stylesheet has loaded.
  void SetStyleSheet(const GURL& url);

  // The user style sheet as a base64 data:// URL.
  GURL user_style_sheet_;

  // Whether the stylesheet has been loaded.
  bool has_loaded_;

  DISALLOW_COPY_AND_ASSIGN(UserStyleSheetLoader);
};

UserStyleSheetLoader::UserStyleSheetLoader()
    : has_loaded_(false) {
}

void UserStyleSheetLoader::NotifyLoaded() {
  if (has_loaded_) {
    NotificationService::current()->Notify(
        NotificationType::USER_STYLE_SHEET_UPDATED,
        Source<UserStyleSheetLoader>(this),
        NotificationService::NoDetails());
  }
}

void UserStyleSheetLoader::OnFilePathChanged(const FilePath& path) {
  LoadStyleSheet(path);
}

void UserStyleSheetLoader::LoadStyleSheet(const FilePath& style_sheet_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // We keep the user style sheet in a subdir so we can watch for changes
  // to the file.
  FilePath style_sheet_dir = style_sheet_file.DirName();
  if (!file_util::DirectoryExists(style_sheet_dir)) {
    if (!file_util::CreateDirectory(style_sheet_dir))
      return;
  }
  // Create the file if it doesn't exist.
  if (!file_util::PathExists(style_sheet_file))
    file_util::WriteFile(style_sheet_file, "", 0);

  std::string css;
  bool rv = file_util::ReadFileToString(style_sheet_file, &css);
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
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &UserStyleSheetLoader::SetStyleSheet,
                        style_sheet_url));
}

void UserStyleSheetLoader::SetStyleSheet(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  has_loaded_ = true;
  user_style_sheet_ = url;
  NotifyLoaded();
}

UserStyleSheetWatcher::UserStyleSheetWatcher(const FilePath& profile_path)
    : profile_path_(profile_path),
      loader_(new UserStyleSheetLoader) {
  // Listen for when the first render view host is created.  If we load
  // too fast, the first tab won't hear the notification and won't get
  // the user style sheet.
  registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
                 NotificationService::AllSources());
}

UserStyleSheetWatcher::~UserStyleSheetWatcher() {
}

void UserStyleSheetWatcher::Init() {
  // Make sure we run on the file thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &UserStyleSheetWatcher::Init));
    return;
  }

  if (!file_watcher_.get()) {
    file_watcher_.reset(new FilePathWatcher);
    FilePath style_sheet_file = profile_path_.AppendASCII(kStyleSheetDir)
                                             .AppendASCII(kUserStyleSheetFile);
    if (!file_watcher_->Watch(style_sheet_file, loader_.get()))
      LOG(ERROR) << "Failed to setup watch for " << style_sheet_file.value();
    loader_->LoadStyleSheet(style_sheet_file);
  }
}

GURL UserStyleSheetWatcher::user_style_sheet() const {
  return loader_->user_style_sheet();
}

void UserStyleSheetWatcher::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  DCHECK(type == NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB);
  loader_->NotifyLoaded();
  registrar_.RemoveAll();
}
