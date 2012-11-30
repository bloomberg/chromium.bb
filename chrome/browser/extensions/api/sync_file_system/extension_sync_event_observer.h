// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"

class Profile;

namespace extensions {

// Observes changes in SyncFileSystem and relays events to JS Extension API.
class ExtensionSyncEventObserver
    : public sync_file_system::SyncEventObserver {
 public:
  explicit ExtensionSyncEventObserver(Profile* profile,
                                      const std::string& service_name);
  virtual ~ExtensionSyncEventObserver();

  virtual void OnSyncStateUpdated(
      const GURL& app_origin,
      sync_file_system::SyncEventObserver::SyncServiceState state,
      const std::string& description) OVERRIDE;

  virtual void OnFileSynced(const fileapi::FileSystemURL& url,
                            fileapi::SyncOperationType operation) OVERRIDE;

 private:
   const std::string& GetExtensionId(const GURL& app_origin);

   Profile* profile_;
   const std::string service_name_;

   DISALLOW_COPY_AND_ASSIGN(ExtensionSyncEventObserver);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_H_
