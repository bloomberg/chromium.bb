// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
#define CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_

#include "base/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class Profile;
class UserStyleSheetLoader;

// Watches the user style sheet file and triggers reloads on the file thread
// whenever the file changes.
class UserStyleSheetWatcher
    : public content::NotificationObserver,
      public RefcountedProfileKeyedService {
 public:
  UserStyleSheetWatcher(Profile* profile, const FilePath& profile_path);

  void Init();

  GURL user_style_sheet() const;

  // content::NotificationObserver interface
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // RefCountedProfileKeyedBase method override.
  virtual void ShutdownOnUIThread() OVERRIDE;

 private:
  friend class base::DeleteHelper<UserStyleSheetWatcher>;

  virtual ~UserStyleSheetWatcher();

  // The profile owning us.
  Profile* profile_;

  // The directory containing User StyleSheets/Custom.css.
  FilePath profile_path_;

  // The loader object.
  scoped_refptr<UserStyleSheetLoader> loader_;

  // Watches for changes to the css file so we can reload the style sheet.
  scoped_ptr<base::FilePathWatcher> file_watcher_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserStyleSheetWatcher);
};

#endif  // CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_H_
