// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_sync_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/history/core/common/pref_names.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_util.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_string.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#include "ios/web_view/internal/passwords/web_view_password_store_factory.h"
#include "ios/web_view/internal/pref_names.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"
#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_invalidation_provider_factory.h"
#include "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {
syncer::ModelTypeSet GetDisabledTypes() {
  syncer::ModelTypeSet disabled_types = syncer::UserTypes();
  disabled_types.Remove(syncer::AUTOFILL);
  disabled_types.Remove(syncer::AUTOFILL_WALLET_DATA);
  disabled_types.Remove(syncer::AUTOFILL_WALLET_METADATA);
  disabled_types.Remove(syncer::AUTOFILL_PROFILE);
  disabled_types.Remove(syncer::PASSWORDS);
  return disabled_types;
}
}  // namespace

// static
std::unique_ptr<WebViewSyncClient> WebViewSyncClient::Create(
    WebViewBrowserState* browser_state) {
  return std::make_unique<WebViewSyncClient>(
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForAccount(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      WebViewPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      WebViewAccountPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      browser_state->GetPrefs(),
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      WebViewModelTypeStoreServiceFactory::GetForBrowserState(browser_state),
      WebViewDeviceInfoSyncServiceFactory::GetForBrowserState(browser_state),
      WebViewProfileInvalidationProviderFactory::GetForBrowserState(
          browser_state)
          ->GetInvalidationService());
}

WebViewSyncClient::WebViewSyncClient(
    autofill::AutofillWebDataService* profile_web_data_service,
    autofill::AutofillWebDataService* account_web_data_service,
    password_manager::PasswordStore* profile_password_store,
    password_manager::PasswordStore* account_password_store,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::ModelTypeStoreService* model_type_store_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    invalidation::InvalidationService* invalidation_service)
    : profile_web_data_service_(profile_web_data_service),
      account_web_data_service_(account_web_data_service),
      profile_password_store_(profile_password_store),
      account_password_store_(account_password_store),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      model_type_store_service_(model_type_store_service),
      device_info_sync_service_(device_info_sync_service),
      invalidation_service_(invalidation_service) {
  component_factory_ =
      std::make_unique<browser_sync::ProfileSyncComponentsFactoryImpl>(
          this, version_info::Channel::STABLE,
          prefs::kSavingBrowserHistoryDisabled,
          base::CreateSingleThreadTaskRunner({web::WebThread::UI}),
          profile_web_data_service_->GetDBTaskRunner(),
          profile_web_data_service_, account_web_data_service_,
          profile_password_store_, account_password_store_,
          /*bookmark_sync_service=*/nullptr);
}

WebViewSyncClient::~WebViewSyncClient() {}

PrefService* WebViewSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return pref_service_;
}

signin::IdentityManager* WebViewSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return identity_manager_;
}

base::FilePath WebViewSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
}

syncer::ModelTypeStoreService* WebViewSyncClient::GetModelTypeStoreService() {
  return model_type_store_service_;
}

syncer::DeviceInfoSyncService* WebViewSyncClient::GetDeviceInfoSyncService() {
  return device_info_sync_service_;
}

bookmarks::BookmarkModel* WebViewSyncClient::GetBookmarkModel() {
  return nullptr;
}

favicon::FaviconService* WebViewSyncClient::GetFaviconService() {
  return nullptr;
}

history::HistoryService* WebViewSyncClient::GetHistoryService() {
  return nullptr;
}

sync_sessions::SessionSyncService* WebViewSyncClient::GetSessionSyncService() {
  return nullptr;
}

send_tab_to_self::SendTabToSelfSyncService*
WebViewSyncClient::GetSendTabToSelfSyncService() {
  return nullptr;
}

base::RepeatingClosure WebViewSyncClient::GetPasswordStateChangedCallback() {
  return base::DoNothing();
}

syncer::DataTypeController::TypeVector
WebViewSyncClient::CreateDataTypeControllers(
    syncer::SyncService* sync_service) {
  return component_factory_->CreateCommonDataTypeControllers(GetDisabledTypes(),
                                                             sync_service);
}

BookmarkUndoService* WebViewSyncClient::GetBookmarkUndoService() {
  return nullptr;
}

invalidation::InvalidationService* WebViewSyncClient::GetInvalidationService() {
  return invalidation_service_;
}

syncer::TrustedVaultClient* WebViewSyncClient::GetTrustedVaultClient() {
  return nullptr;
}

scoped_refptr<syncer::ExtensionsActivity>
WebViewSyncClient::GetExtensionsActivity() {
  return nullptr;
}

base::WeakPtr<syncer::SyncableService>
WebViewSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  NOTREACHED();
  return base::WeakPtr<syncer::SyncableService>();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
WebViewSyncClient::GetControllerDelegateForModelType(syncer::ModelType type) {
  NOTREACHED();
  return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
}

scoped_refptr<syncer::ModelSafeWorker>
WebViewSyncClient::CreateModelWorkerForGroup(syncer::ModelSafeGroup group) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  switch (group) {
    case syncer::GROUP_PASSIVE:
      return new syncer::PassiveModelWorker();
    default:
      return nullptr;
  }
}

syncer::SyncApiComponentFactory*
WebViewSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

syncer::SyncTypePreferenceProvider* WebViewSyncClient::GetPreferenceProvider() {
  return nullptr;
}

}  // namespace ios_web_view
