// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_MOCK_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_MOCK_H__

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/data_type_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {
class AssociatorInterface;
class ChangeProcessor;
class DataTypeEncryptionHandler;
class FailedDataTypesHandler;
}

class ProfileSyncComponentsFactoryMock : public ProfileSyncComponentsFactory {
 public:
  ProfileSyncComponentsFactoryMock();
  ProfileSyncComponentsFactoryMock(
      browser_sync::AssociatorInterface* model_associator,
      browser_sync::ChangeProcessor* change_processor);
  virtual ~ProfileSyncComponentsFactoryMock();

  MOCK_METHOD1(RegisterDataTypes, void(ProfileSyncService*));
  MOCK_METHOD6(CreateDataTypeManager,
               browser_sync::DataTypeManager*(
                   const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&,
                   const browser_sync::DataTypeController::TypeMap*,
                   const browser_sync::DataTypeEncryptionHandler*,
                   browser_sync::SyncBackendHost*,
                   browser_sync::DataTypeManagerObserver* observer,
                   browser_sync::FailedDataTypesHandler*
                       failed_datatypes_handler));
  MOCK_METHOD5(CreateSyncBackendHost,
               browser_sync::SyncBackendHost*(
                   const std::string& name,
                   Profile* profile,
                   invalidation::InvalidationService* invalidator,
                   const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
                   const base::FilePath& sync_folder));
  MOCK_METHOD1(GetSyncableServiceForType,
               base::WeakPtr<syncer::SyncableService>(syncer::ModelType));
  virtual scoped_ptr<syncer::AttachmentService> CreateAttachmentService(
      const syncer::UserShare& user_share,
      syncer::AttachmentService::Delegate* delegate) OVERRIDE;
  MOCK_METHOD2(CreateBookmarkSyncComponents,
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
