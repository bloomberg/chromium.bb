// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_CHANGE_PROCESSOR_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/extension_sync_traits.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_type.h"

class ExtensionService;
class NotificationDetails;
class NotificationSource;
class Profile;

namespace browser_sync {

class UnrecoverableErrorHandler;

// This class is responsible for taking changes from the
// ExtensionService and applying them to the sync_api 'syncable'
// model, and vice versa. All operations and use of this class are
// from the UI thread.
class ExtensionChangeProcessor : public ChangeProcessor,
                                 public NotificationObserver {
 public:
  // Does not take ownership of |error_handler|.
  ExtensionChangeProcessor(
      const ExtensionSyncTraits& traits,
      UnrecoverableErrorHandler* error_handler);
  virtual ~ExtensionChangeProcessor();

  // NotificationObserver implementation.
  // BrowserExtensionProvider -> sync_api model change application.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ChangeProcessor implementation.
  // sync_api model -> BrowserExtensionProvider change application.
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
  const ExtensionSyncTraits traits_;
  // Owner of the ExtensionService.  Non-NULL iff |running()| is true.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_CHANGE_PROCESSOR_H_
