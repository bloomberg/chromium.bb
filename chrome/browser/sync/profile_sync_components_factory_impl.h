// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "google_apis/gaia/oauth2_token_service.h"

class Profile;

namespace base {
class CommandLine;
}

namespace extensions {
class ExtensionSystem;
}

class ProfileSyncComponentsFactoryImpl : public ProfileSyncComponentsFactory {
 public:
  // Constructs a ProfileSyncComponentsFactoryImpl.
  //
  // |sync_service_url| is the base URL of the sync server.
  //
  // |token_service| must outlive the ProfileSyncComponentsFactoryImpl.
  //
  // |url_request_context_getter| must outlive the
  // ProfileSyncComponentsFactoryImpl.
  ProfileSyncComponentsFactoryImpl(
      Profile* profile,
      base::CommandLine* command_line,
      const GURL& sync_service_url,
      OAuth2TokenService* token_service,
      net::URLRequestContextGetter* url_request_context_getter);
  virtual ~ProfileSyncComponentsFactoryImpl();

  virtual void RegisterDataTypes(ProfileSyncService* pss) OVERRIDE;

  virtual sync_driver::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const sync_driver::DataTypeController::TypeMap* controllers,
      const sync_driver::DataTypeEncryptionHandler* encryption_handler,
      browser_sync::SyncBackendHost* backend,
      sync_driver::DataTypeManagerObserver* observer)
          OVERRIDE;

  virtual browser_sync::SyncBackendHost* CreateSyncBackendHost(
      const std::string& name,
      Profile* profile,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder) OVERRIDE;

  virtual scoped_ptr<sync_driver::LocalDeviceInfoProvider>
      CreateLocalDeviceInfoProvider() OVERRIDE;

  virtual base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) OVERRIDE;
  virtual scoped_ptr<syncer::AttachmentService> CreateAttachmentService(
      const scoped_refptr<syncer::AttachmentStore>& attachment_store,
      const syncer::UserShare& user_share,
      syncer::AttachmentService::Delegate* delegate) OVERRIDE;

  // Legacy datatypes that need to be converted to the SyncableService API.
  virtual SyncComponents CreateBookmarkSyncComponents(
      ProfileSyncService* profile_sync_service,
      sync_driver::DataTypeErrorHandler* error_handler) OVERRIDE;
  virtual SyncComponents CreateTypedUrlSyncComponents(
      ProfileSyncService* profile_sync_service,
      history::HistoryBackend* history_backend,
      sync_driver::DataTypeErrorHandler* error_handler) OVERRIDE;

 private:
  // Register data types which are enabled on desktop platforms only.
  // |disabled_types| and |enabled_types| correspond only to those types
  // being explicitly enabled/disabled by the command line.
  void RegisterDesktopDataTypes(syncer::ModelTypeSet disabled_types,
                                syncer::ModelTypeSet enabled_types,
                                ProfileSyncService* pss);
  // Register data types which are enabled on both desktop and mobile.
  // |disabled_types| and |enabled_types| correspond only to those types
  // being explicitly enabled/disabled by the command line.
  void RegisterCommonDataTypes(syncer::ModelTypeSet disabled_types,
                               syncer::ModelTypeSet enabled_types,
                               ProfileSyncService* pss);
  // Used to bind a callback to give to DataTypeControllers to disable
  // data types.
  sync_driver::DataTypeController::DisableTypeCallback
      MakeDisableCallbackFor(syncer::ModelType type);
  void DisableBrokenType(syncer::ModelType type,
                         const tracked_objects::Location& from_here,
                         const std::string& message);

  Profile* profile_;
  base::CommandLine* command_line_;
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  const GURL sync_service_url_;
  OAuth2TokenService* const token_service_;
  net::URLRequestContextGetter* const url_request_context_getter_;

  base::WeakPtrFactory<ProfileSyncComponentsFactoryImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncComponentsFactoryImpl);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
