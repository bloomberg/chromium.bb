// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
#define CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
#pragma once

#include "base/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class Profile;
class UserStyleSheetLoader;

// Watches the user style sheet file and triggers reloads on the file thread
// whenever the file changes.
class UserStyleSheetWatcher
    : public base::RefCountedThreadSafe<
          UserStyleSheetWatcher,
          content::BrowserThread::DeleteOnUIThread>,
      public content::NotificationObserver {
 public:
  UserStyleSheetWatcher(Profile* profile, const FilePath& profile_path);

  void Init();

  GURL user_style_sheet() const;

  // content::NotificationObserver interface
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class DeleteTask<UserStyleSheetWatcher>;

  virtual ~UserStyleSheetWatcher();

  // The profile owning us.
  Profile* profile_;

  // The directory containing User StyleSheets/Custom.css.
  FilePath profile_path_;

  // The loader object.
  scoped_refptr<UserStyleSheetLoader> loader_;

  // Watches for changes to the css file so we can reload the style sheet.
  scoped_ptr<base::files::FilePathWatcher> file_watcher_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserStyleSheetWatcher);
};

#endif  // CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
