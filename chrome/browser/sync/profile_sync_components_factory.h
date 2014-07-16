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
class AssociatorInterface;
class ChangeProcessor;
class DataTypeEncryptionHandler;
class DataTypeManager;
class DataTypeManagerObserver;
class FailedDataTypesHandler;
class GenericChangeProcessor;
class LocalDeviceInfoProvider;
class SyncBackendHost;
class DataTypeErrorHandler;
}  // namespace browser_sync

namespace sync_driver {
class SyncPrefs;
}

namespace syncer {
class DataTypeDebugInfoListener;
class SyncableService;
}

namespace history {
class HistoryBackend;
}

// Factory class for all profile sync related classes.
class ProfileSyncComponentsFactory
    : public browser_sync::SyncApiComponentFactory {
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
    browser_sync::AssociatorInterface* model_associator;
    browser_sync::ChangeProcessor* change_processor;
    SyncComponents(browser_sync::AssociatorInterface* ma,
                   browser_sync::ChangeProcessor* cp)
        : model_associator(ma), change_processor(cp) {}
  };

  virtual ~ProfileSyncComponentsFactory() OVERRIDE {}

  // Creates and registers enabled datatypes with the provided
  // ProfileSyncService.
  virtual void RegisterDataTypes(ProfileSyncService* pss) = 0;

  // Instantiates a new DataTypeManager with a SyncBackendHost, a list of data
  // type controllers and a DataTypeManagerObserver.  The return pointer is
  // owned by the caller.
  virtual browser_sync::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const browser_sync::DataTypeController::TypeMap* controllers,
      const browser_sync::DataTypeEncryptionHandler* encryption_handler,
      browser_sync::SyncBackendHost* backend,
      browser_sync::DataTypeManagerObserver* observer,
      browser_sync::FailedDataTypesHandler* failed_data_types_handler) = 0;

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
      browser_sync::DataTypeErrorHandler* error_handler) = 0;
  virtual SyncComponents CreateTypedUrlSyncComponents(
      ProfileSyncService* profile_sync_service,
      history::HistoryBackend* history_backend,
      browser_sync::DataTypeErrorHandler* error_handler) = 0;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_H__
