// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_H__

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/invalidation_service.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/data_type_error_handler.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/internal_api/public/util/weak_handle.h"

class PasswordStore;
class Profile;
class ProfileSyncService;

namespace browser_sync {
class LocalDeviceInfoProvider;
class SyncBackendHost;
}  // namespace browser_sync

namespace sync_driver {
class AssociatorInterface;
class ChangeProcessor;
class DataTypeEncryptionHandler;
class DataTypeErrorHandler;
class DataTypeManager;
class DataTypeManagerObserver;
class FailedDataTypesHandler;
class GenericChangeProcessor;
class SyncPrefs;
}  // namespace sync_driver

namespace syncer {
class DataTypeDebugInfoListener;
class SyncableService;
}  // namespace syncer

namespace history {
class HistoryBackend;
}  // namespace history

// Factory class for all profile sync related classes.
class ProfileSyncComponentsFactory
    : public sync_driver::SyncApiComponentFactory {
 public:
  // The various factory methods for the data type model associators
  // and change processors all return this struct.  This is needed
  // because the change processors typically require a type-specific
  // model associator at construction time.
  //
  // Note: This interface is deprecated in favor of the SyncableService API.
  // New datatypes that do not live on the UI thread should directly return a
  // weak pointer to a syncer::SyncableService. All others continue to return
  // SyncComponents. It is safe to assume that the factory methods below are
  // called on the same thread in which the datatype resides.
  //
  // TODO(zea): Have all datatypes using the new API switch to returning
  // SyncableService weak pointers instead of SyncComponents (crbug.com/100114).
  struct SyncComponents {
    sync_driver::AssociatorInterface* model_associator;
    sync_driver::ChangeProcessor* change_processor;
    SyncComponents(sync_driver::AssociatorInterface* ma,
                   sync_driver::ChangeProcessor* cp)
        : model_associator(ma), change_processor(cp) {}
  };

  virtual ~ProfileSyncComponentsFactory() OVERRIDE {}

  // Creates and registers enabled datatypes with the provided
  // ProfileSyncService.
  virtual void RegisterDataTypes(ProfileSyncService* pss) = 0;

  // Instantiates a new DataTypeManager with a SyncBackendHost, a list of data
  // type controllers and a DataTypeManagerObserver.  The return pointer is
  // owned by the caller.
  virtual sync_driver::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const sync_driver::DataTypeController::TypeMap* controllers,
      const sync_driver::DataTypeEncryptionHandler* encryption_handler,
      browser_sync::SyncBackendHost* backend,
      sync_driver::DataTypeManagerObserver* observer,
      sync_driver::FailedDataTypesHandler* failed_data_types_handler) = 0;

  // Creating this in the factory helps us mock it out in testing.
  virtual browser_sync::SyncBackendHost* CreateSyncBackendHost(
      const std::string& name,
      Profile* profile,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder) = 0;

  // Creating this in the factory helps us mock it out in testing.
  virtual scoped_ptr<browser_sync::LocalDeviceInfoProvider>
      CreateLocalDeviceInfoProvider() = 0;

  // Legacy datatypes that need to be converted to the SyncableService API.
  virtual SyncComponents CreateBookmarkSyncComponents(
      ProfileSyncService* profile_sync_service,
      sync_driver::DataTypeErrorHandler* error_handler) = 0;
  virtual SyncComponents CreateTypedUrlSyncComponents(
      ProfileSyncService* profile_sync_service,
      history::HistoryBackend* history_backend,
      sync_driver::DataTypeErrorHandler* error_handler) = 0;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_H__
