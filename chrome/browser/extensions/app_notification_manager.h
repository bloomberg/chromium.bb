// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_MANAGER_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/app_notification.h"
#include "chrome/browser/extensions/app_notification_storage.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Profile;

// This class keeps track of notifications for installed apps.
class AppNotificationManager
    : public base::RefCountedThreadSafe<AppNotificationManager>,
      public NotificationObserver {
 public:
  explicit AppNotificationManager(Profile* profile);

  // Starts up the process of reading from persistent storage.
  void Init();

  // Takes ownership of |notification|.
  void Add(const std::string& extension_id, AppNotification* item);

  const AppNotificationList* GetAll(const std::string& extension_id);

  // Returns the most recently added notification for |extension_id| if there
  // are any, or NULL otherwise.
  const AppNotification* GetLast(const std::string& extension_id);

  // Clears all notifications for |extension_id|.
  void ClearAll(const std::string& extension_id);

  // Implementing NotificationObserver interface.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<AppNotificationManager>;

  // Maps extension id to a list of notifications for that extension.
  typedef std::map<std::string, AppNotificationList> NotificationMap;

  virtual ~AppNotificationManager();

  // Starts loading storage_ using |storage_path|.
  void LoadOnFileThread(const FilePath& storage_path);

  // Called on the UI thread to handle the loaded results from storage_.
  void HandleLoadResults(const NotificationMap& map);

  void SaveOnFileThread(const std::string& extension_id,
                        const AppNotificationList& list);

  void DeleteOnFileThread(const std::string& extension_id);

  Profile* profile_;
  NotificationRegistrar registrar_;
  NotificationMap notifications_;

  // This should only be used on the FILE thread.
  scoped_ptr<AppNotificationStorage> storage_;

  DISALLOW_COPY_AND_ASSIGN(AppNotificationManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_MANAGER_H_
