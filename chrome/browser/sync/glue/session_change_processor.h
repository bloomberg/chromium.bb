// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SESSION_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_SESSION_CHANGE_PROCESSOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"

class Profile;

namespace browser_sync {

class SessionModelAssociator;
class DataTypeErrorHandler;

// This class is responsible for taking changes from the
// SessionService and applying them to the sync API 'syncable'
// model, and vice versa. All operations and use of this class are
// from the UI thread.
class SessionChangeProcessor : public ChangeProcessor,
                               public content::NotificationObserver {
 public:
  // Does not take ownership of either argument.
  SessionChangeProcessor(
      DataTypeErrorHandler* error_handler,
      SessionModelAssociator* session_model_associator);
  // For testing only.
  SessionChangeProcessor(
      DataTypeErrorHandler* error_handler,
      SessionModelAssociator* session_model_associator,
      bool setup_for_test);
  virtual ~SessionChangeProcessor();

  // content::NotificationObserver implementation.
  // BrowserSessionProvider -> sync API model change application.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ChangeProcessor implementation.
  // sync API model -> BrowserSessionProvider change application.
  virtual void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 model_version,
      const syncer::ImmutableChangeRecordList& changes) OVERRIDE;

 protected:
  // ChangeProcessor implementation.
  virtual void StartImpl(Profile* profile) OVERRIDE;

 private:
  friend class ScopedStopObserving<SessionChangeProcessor>;

  void StartObserving();
  void StopObserving();

  SessionModelAssociator* session_model_associator_;
  content::NotificationRegistrar notification_registrar_;

  // Profile being synced. Non-null if |running()| is true.
  Profile* profile_;

  // To bypass some checks/codepaths not applicable in tests.
  bool setup_for_test_;

  DISALLOW_COPY_AND_ASSIGN(SessionChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SESSION_CHANGE_PROCESSOR_H_
