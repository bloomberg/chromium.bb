// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_THEME_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_THEME_CHANGE_PROCESSOR_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_type.h"

class NotificationDetails;
class NotificationSource;
class Profile;

namespace browser_sync {

class UnrecoverableErrorHandler;

// This class is responsible for taking changes from the
// BrowserThemeProvider and applying them to the sync_api 'syncable'
// model, and vice versa. All operations and use of this class are
// from the UI thread.
class ThemeChangeProcessor : public ChangeProcessor,
                             public NotificationObserver {
 public:
  explicit ThemeChangeProcessor(UnrecoverableErrorHandler* error_handler);
  virtual ~ThemeChangeProcessor();

  // NotificationObserver implementation.
  // BrowserThemeProvider -> sync_api model change application.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ChangeProcessor implementation.
  // sync_api model -> BrowserThemeProvider change application.
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count);

 protected:
  // ChangeProcessor implementation.
  virtual void StartImpl(Profile* profile);
  virtual void StopImpl();

 private:
  void StartObserving();
  void StopObserving();

  NotificationRegistrar notification_registrar_;
  // Owner of the BrowserThemeProvider.  Non-NULL iff |running()| is
  // true.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ThemeChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_THEME_CHANGE_PROCESSOR_H_
