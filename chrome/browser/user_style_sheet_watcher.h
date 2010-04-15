// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
#define CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_

#include "base/file_path.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/file_watcher.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

// This loads the user style sheet on the file thread and sends a notification
// when the style sheet is loaded.
class UserStyleSheetWatcher
    : public base::RefCountedThreadSafe<UserStyleSheetWatcher,
                                        ChromeThread::DeleteOnUIThread>,
      public NotificationObserver,
      public FileWatcher::Delegate {
 public:
  explicit UserStyleSheetWatcher(const FilePath& profile_path);
  virtual ~UserStyleSheetWatcher() {}

  void Init();

  GURL user_style_sheet() const {
    return user_style_sheet_;
  }

  // NotificationObserver interface
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // FileWatcher::Delegate interface
  virtual void OnFileChanged(const FilePath& path);

 private:
  // Load the user style sheet on the file thread and convert it to a
  // base64 URL.  Posts the base64 URL back to the UI thread.
  void LoadStyleSheet(const FilePath& profile_path);

  void SetStyleSheet(const GURL& url);

  // The directory containing User StyleSheets/Custom.css.
  FilePath profile_path_;

  // The user style sheet as a base64 data:// URL.
  GURL user_style_sheet_;

  // Watches for changes to the css file so we can reload the style sheet.
  scoped_ptr<FileWatcher> file_watcher_;

  NotificationRegistrar registrar_;
  bool has_loaded_;

  DISALLOW_COPY_AND_ASSIGN(UserStyleSheetWatcher);
};

#endif  // CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
