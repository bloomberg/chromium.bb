// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_MOCK_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_MOCK_H__

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {
class AssociatorInterface;
class ChangeProcessor;
}

class ProfileSyncComponentsFactoryMock : public ProfileSyncComponentsFactory {
 public:
  ProfileSyncComponentsFactoryMock();
  ProfileSyncComponentsFactoryMock(
      browser_sync::AssociatorInterface* model_associator,
      browser_sync::ChangeProcessor* change_processor);
  virtual ~ProfileSyncComponentsFactoryMock();

  MOCK_METHOD1(RegisterDataTypes, void(ProfileSyncService*));
  MOCK_METHOD5(CreateDataTypeManager,
               browser_sync::DataTypeManager*(
                   const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&,
                   browser_sync::SyncBackendHost*,
                   const browser_sync::DataTypeController::TypeMap*,
                   browser_sync::DataTypeManagerObserver* observer,
                   const FailedDatatypesHandler* failed_datatypes_handler));
  MOCK_METHOD4(CreateGenericChangeProcessor,
      browser_sync::GenericChangeProcessor*(
          ProfileSyncService* profile_sync_service,
          browser_sync::DataTypeErrorHandler* error_handler,
          const base::WeakPtr<syncer::SyncableService>& local_service,
          const base::WeakPtr<syncer::SyncMergeResult>& merge_result));
  MOCK_METHOD0(CreateSharedChangeProcessor,
      browser_sync::SharedChangeProcessor*());
  MOCK_METHOD1(GetSyncableServiceForType,
               base::WeakPtr<syncer::SyncableService>(syncer::ModelType));
  MOCK_METHOD2(CreateBookmarkSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::DataTypeErrorHandler* error_handler));
  MOCK_METHOD3(CreatePasswordSyncComponents,
               SyncComponents(
                   ProfileSyncService* profile_sync_service,
                   PasswordStore* password_store,
                   browser_sync::DataTypeErrorHandler* error_handler));
  MOCK_METHOD2(CreateSessionSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::DataTypeErrorHandler* error_handler));
#if defined(ENABLE_THEMES)
  MOCK_METHOD2(CreateThemeSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     browser_sync::DataTypeErrorHandler* error_handler));
#endif
  MOCK_METHOD3(CreateTypedUrlSyncComponents,
               SyncComponents(
                   ProfileSyncService* profile_sync_service,
                   history::HistoryBackend* history_backend,
                   browser_sync::DataTypeErrorHandler* error_handler));

 private:
  SyncComponents MakeSyncComponents();

  scoped_ptr<browser_sync::AssociatorInterface> model_associator_;
  scoped_ptr<browser_sync::ChangeProcessor> change_processor_;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_MOCK_H__
