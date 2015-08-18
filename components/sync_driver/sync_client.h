// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_driver/profile_sync_components_factory.h"
#include "sync/internal_api/public/base/model_type.h"

namespace autofill {
class AutofillWebDataService;
class AutocompleteSyncableService;
class PersonalDataManager;
}  // namespace autofill

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace history {
class HistoryService;
}  // namespace history

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

class PrefService;

namespace sync_driver {

// Interface for clients of the Sync API to plumb through necessary dependent
// components. This interface is purely for abstracting dependencies, and
// should not contain any non-trivial functional logic.
//
// Note: on some platforms, getters might return nullptr. Callers are expected
// to handle these scenarios gracefully.
// TODO(zea): crbug.com/512768 Remove the ProfileSyncComponentsFactory
// dependency once everything uses SyncClient instead, then have the SyncClient
// include a GetSyncApiComponentsFactory getter.
class SyncClient : public ProfileSyncComponentsFactory {
 public:
  SyncClient();

  // Returns the current profile's preference service.
  virtual PrefService* GetPrefService() = 0;

  // DataType specific service getters.
  virtual bookmarks::BookmarkModel* GetBookmarkModel() = 0;
  virtual history::HistoryService* GetHistoryService() = 0;
  virtual scoped_refptr<password_manager::PasswordStore> GetPasswordStore() = 0;
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;
  virtual scoped_refptr<autofill::AutofillWebDataService>
  GetWebDataService() = 0;

 protected:
  ~SyncClient() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncClient);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_H_
