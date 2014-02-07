// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_CHANGE_PROCESSOR_H_

#include "chrome/browser/sync/glue/change_processor.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/password_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync_driver/data_type_error_handler.h"

class PasswordStore;

namespace base {
class MessageLoop;
}

namespace browser_sync {

class DataTypeErrorHandler;

// This class is responsible for taking changes from the password backend and
// applying them to the sync API 'syncable' model, and vice versa. All
// operations and use of this class are from the DB thread on Windows and Linux,
// or the password thread on Mac.
class PasswordChangeProcessor : public ChangeProcessor,
                                public PasswordStore::Observer {
 public:
  PasswordChangeProcessor(PasswordModelAssociator* model_associator,
                          PasswordStore* password_store,
                          DataTypeErrorHandler* error_handler);
  virtual ~PasswordChangeProcessor();

  // PasswordStore::Observer implementation:
  // Passwords -> sync API model change application.
  virtual void OnLoginsChanged(const PasswordStoreChangeList& changes) OVERRIDE;

  // sync API model -> WebDataService change application.
  virtual void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 model_version,
      const syncer::ImmutableChangeRecordList& changes) OVERRIDE;

  // Commit changes buffered during ApplyChanges. We must commit them to the
  // password store only after the sync API transaction is released, else there
  // is risk of deadlock due to the password store posting tasks to the UI
  // thread (http://crbug.com/70658).
  virtual void CommitChangesFromSyncModel() OVERRIDE;

  // Stop processing changes and wait for being destroyed.
  void Disconnect();

 protected:

  virtual void StartImpl(Profile* profile) OVERRIDE;

 private:
  friend class ScopedStopObserving<PasswordChangeProcessor>;

  void StartObserving();
  void StopObserving();

  // Registers to observe password change for the first time.
  void InitObserving();

  // The two models should be associated according to this ModelAssociator.
  PasswordModelAssociator* model_associator_;

  // The model we are processing changes from.  This is owned by the
  // WebDataService which is kept alive by our data type controller
  // holding a reference.
  scoped_refptr<PasswordStore> password_store_;

  // Buffers used between ApplyChangesFromSyncModel and
  // CommitChangesFromSyncModel.
  PasswordModelAssociator::PasswordVector new_passwords_;
  PasswordModelAssociator::PasswordVector updated_passwords_;
  PasswordModelAssociator::PasswordVector deleted_passwords_;

  base::MessageLoop* expected_loop_;

  // If disconnected is true, local/sync changes are dropped without modifying
  // sync/local models.
  bool disconnected_;
  base::Lock disconnect_lock_;

  DISALLOW_COPY_AND_ASSIGN(PasswordChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_CHANGE_PROCESSOR_H_
