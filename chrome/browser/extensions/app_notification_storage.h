// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_STORAGE_H__
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_STORAGE_H__

#include <set>

#include "chrome/browser/extensions/app_notification.h"

namespace base {
class FilePath;
}

namespace extensions {

// Represents storage for app notifications for a particular extension id.
//
// IMPORTANT NOTE: Instances of this class should only be used on the FILE
// thread.
class AppNotificationStorage {
 public:
  // Must be called on the FILE thread. The storage will be created at |path|.
  static AppNotificationStorage* Create(const base::FilePath& path);

  virtual ~AppNotificationStorage();

  // Get the set of extension id's that have entries, putting them into
  // |result|.
  virtual bool GetExtensionIds(std::set<std::string>* result) = 0;

  // Gets the list of stored notifications for extension_id. On success, writes
  // results into |result|. On error, returns false.
  virtual bool Get(const std::string& extension_id,
                   AppNotificationList* result) = 0;

  // Writes the |list| for |extension_id| into storage.
  virtual bool Set(const std::string& extension_id,
                   const AppNotificationList& list) = 0;

  // Deletes all data for |extension_id|.
  virtual bool Delete(const std::string& extension_id) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_STORAGE_H__
