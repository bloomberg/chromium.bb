// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
#define CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
#pragma once

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/file_path_watcher/file_path_watcher.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class UserStyleSheetLoader;

// Watches the user style sheet file and triggers reloads on the file thread
// whenever the file changes.
class UserStyleSheetWatcher
    : public base::RefCountedThreadSafe<UserStyleSheetWatcher,
                                        BrowserThread::DeleteOnUIThread>,
      public NotificationObserver {
 public:
  explicit UserStyleSheetWatcher(const FilePath& profile_path);

  void Init();

  GURL user_style_sheet() const;

  // NotificationObserver interface
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<UserStyleSheetWatcher>;

  virtual ~UserStyleSheetWatcher();

  // The directory containing User StyleSheets/Custom.css.
  FilePath profile_path_;

  // The loader object.
  scoped_refptr<UserStyleSheetLoader> loader_;

  // Watches for changes to the css file so we can reload the style sheet.
  scoped_ptr<FilePathWatcher> file_watcher_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserStyleSheetWatcher);
};

#endif  // CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
