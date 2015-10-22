// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "sync/internal_api/public/base/model_type.h"
#include "url/gurl.h"

class OAuth2TokenService;
class Profile;

namespace base {
class CommandLine;
}

namespace net {
class URLRequestContextGetter;
}

class ProfileSyncComponentsFactoryImpl
    : public sync_driver::SyncApiComponentFactory {
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
  ~ProfileSyncComponentsFactoryImpl() override;

  void RegisterDataTypes(sync_driver::SyncClient* sync_client) override;

  sync_driver::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const sync_driver::DataTypeController::TypeMap* controllers,
      const sync_driver::DataTypeEncryptionHandler* encryption_handler,
      browser_sync::SyncBackendHost* backend,
      sync_driver::DataTypeManagerObserver* observer) override;

  browser_sync::SyncBackendHost* CreateSyncBackendHost(
      const std::string& name,
      sync_driver::SyncClient* sync_client,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder) override;

  scoped_ptr<sync_driver::LocalDeviceInfoProvider>
  CreateLocalDeviceInfoProvider() override;

  scoped_ptr<syncer::AttachmentService> CreateAttachmentService(
      scoped_ptr<syncer::AttachmentStoreForSync> attachment_store,
      const syncer::UserShare& user_share,
      const std::string& store_birthday,
      syncer::ModelType model_type,
      syncer::AttachmentService::Delegate* delegate) override;

  // Legacy datatypes that need to be converted to the SyncableService API.
  sync_driver::SyncApiComponentFactory::SyncComponents
  CreateBookmarkSyncComponents(
      sync_driver::SyncService* sync_service,
      sync_driver::DataTypeErrorHandler* error_handler) override;
  sync_driver::SyncApiComponentFactory::SyncComponents
  CreateTypedUrlSyncComponents(
      sync_driver::SyncService* sync_service,
      history::HistoryBackend* history_backend,
      sync_driver::DataTypeErrorHandler* error_handler) override;

 private:
  // Register data types which are enabled on desktop platforms only.
  // |disabled_types| and |enabled_types| correspond only to those types
  // being explicitly enabled/disabled by the command line.
  void RegisterDesktopDataTypes(syncer::ModelTypeSet disabled_types,
                                syncer::ModelTypeSet enabled_types,
                                sync_driver::SyncClient* sync_client);

  // Register data types which are enabled on both desktop and mobile.
  // |disabled_types| and |enabled_types| correspond only to those types
  // being explicitly enabled/disabled by the command line.
  void RegisterCommonDataTypes(syncer::ModelTypeSet disabled_types,
                               syncer::ModelTypeSet enabled_types,
                               sync_driver::SyncClient* sync_client);

  void DisableBrokenType(syncer::ModelType type,
                         const tracked_objects::Location& from_here,
                         const std::string& message);

  Profile* profile_;
  base::CommandLine* command_line_;

  const GURL sync_service_url_;
  OAuth2TokenService* const token_service_;
  net::URLRequestContextGetter* const url_request_context_getter_;

  base::WeakPtrFactory<ProfileSyncComponentsFactoryImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncComponentsFactoryImpl);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
