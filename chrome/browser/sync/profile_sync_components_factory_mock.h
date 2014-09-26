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

namespace sync_driver {
class AssociatorInterface;
class ChangeProcessor;
class DataTypeEncryptionHandler;
class DataTypeStatusTable;
}

class ProfileSyncComponentsFactoryMock : public ProfileSyncComponentsFactory {
 public:
  ProfileSyncComponentsFactoryMock();
  ProfileSyncComponentsFactoryMock(
      sync_driver::AssociatorInterface* model_associator,
      sync_driver::ChangeProcessor* change_processor);
  virtual ~ProfileSyncComponentsFactoryMock();

  MOCK_METHOD1(RegisterDataTypes, void(ProfileSyncService*));
  MOCK_METHOD5(CreateDataTypeManager,
               sync_driver::DataTypeManager*(
                   const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&,
                   const sync_driver::DataTypeController::TypeMap*,
                   const sync_driver::DataTypeEncryptionHandler*,
                   browser_sync::SyncBackendHost*,
                   sync_driver::DataTypeManagerObserver* observer));
  MOCK_METHOD5(CreateSyncBackendHost,
               browser_sync::SyncBackendHost*(
                   const std::string& name,
                   Profile* profile,
                   invalidation::InvalidationService* invalidator,
                   const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
                   const base::FilePath& sync_folder));

  virtual scoped_ptr<sync_driver::LocalDeviceInfoProvider>
      CreateLocalDeviceInfoProvider() OVERRIDE;
  void SetLocalDeviceInfoProvider(
      scoped_ptr<sync_driver::LocalDeviceInfoProvider> local_device);

  MOCK_METHOD1(GetSyncableServiceForType,
               base::WeakPtr<syncer::SyncableService>(syncer::ModelType));
  virtual scoped_ptr<syncer::AttachmentService> CreateAttachmentService(
      const scoped_refptr<syncer::AttachmentStore>& attachment_store,
      const syncer::UserShare& user_share,
      syncer::AttachmentService::Delegate* delegate) OVERRIDE;
  MOCK_METHOD2(CreateBookmarkSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     sync_driver::DataTypeErrorHandler* error_handler));
#if defined(ENABLE_THEMES)
  MOCK_METHOD2(CreateThemeSyncComponents,
      SyncComponents(ProfileSyncService* profile_sync_service,
                     sync_driver::DataTypeErrorHandler* error_handler));
#endif
  MOCK_METHOD3(CreateTypedUrlSyncComponents,
               SyncComponents(
                   ProfileSyncService* profile_sync_service,
                   history::HistoryBackend* history_backend,
                   sync_driver::DataTypeErrorHandler* error_handler));

 private:
  SyncComponents MakeSyncComponents();

  scoped_ptr<sync_driver::AssociatorInterface> model_associator_;
  scoped_ptr<sync_driver::ChangeProcessor> change_processor_;
  // LocalDeviceInfoProvider is initially owned by this class,
  // transferred to caller when CreateLocalDeviceInfoProvider is called.
  scoped_ptr<sync_driver::LocalDeviceInfoProvider> local_device_;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_MOCK_H__
