// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_components_factory_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_wallet_data_type_controller.h"
#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_data_type_controller.h"
#include "components/autofill/core/browser/webdata/autofill_profile_data_type_controller.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/web_data_model_type_controller.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/history/core/browser/history_delete_directives_data_type_controller.h"
#include "components/history/core/browser/typed_url_data_type_controller.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/sync/browser/password_data_type_controller.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_switches.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/device_info/device_info_data_type_controller.h"
#include "components/sync/device_info/local_device_info_provider_impl.h"
#include "components/sync/driver/async_directory_type_controller.h"
#include "components/sync/driver/data_type_manager_impl.h"
#include "components/sync/driver/glue/sync_backend_host_impl.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/proxy_data_type_controller.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/attachments/attachment_downloader.h"
#include "components/sync/engine/attachments/attachment_uploader.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/model/attachments/attachment_service.h"
#include "components/sync_bookmarks/bookmark_change_processor.h"
#include "components/sync_bookmarks/bookmark_data_type_controller.h"
#include "components/sync_bookmarks/bookmark_model_associator.h"
#include "components/sync_sessions/session_data_type_controller.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_request.h"
#include "net/url_request/url_request_context_getter.h"

using base::FeatureList;
using bookmarks::BookmarkModel;
using sync_bookmarks::BookmarkChangeProcessor;
using sync_bookmarks::BookmarkDataTypeController;
using sync_bookmarks::BookmarkModelAssociator;
using sync_sessions::SessionDataTypeController;
using syncer::AsyncDirectoryTypeController;
using syncer::DataTypeController;
using syncer::DataTypeManager;
using syncer::DataTypeManagerImpl;
using syncer::DataTypeManagerObserver;
using syncer::DeviceInfoDataTypeController;
using syncer::ModelTypeController;
using syncer::ProxyDataTypeController;

namespace browser_sync {

namespace {

syncer::ModelTypeSet GetDisabledTypesFromCommandLine(
    const base::CommandLine& command_line) {
  syncer::ModelTypeSet disabled_types;
  std::string disabled_types_str =
      command_line.GetSwitchValueASCII(switches::kDisableSyncTypes);

  disabled_types = syncer::ModelTypeSetFromString(disabled_types_str);
  return disabled_types;
}

syncer::ModelTypeSet GetEnabledTypesFromCommandLine(
    const base::CommandLine& command_line) {
  return syncer::ModelTypeSet();
}

}  // namespace

ProfileSyncComponentsFactoryImpl::ProfileSyncComponentsFactoryImpl(
    syncer::SyncClient* sync_client,
    version_info::Channel channel,
    const std::string& version,
    bool is_tablet,
    const base::CommandLine& command_line,
    const char* history_disabled_pref,
    const GURL& sync_service_url,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* url_request_context_getter,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service,
    const scoped_refptr<password_manager::PasswordStore>& password_store)
    : sync_client_(sync_client),
      channel_(channel),
      version_(version),
      is_tablet_(is_tablet),
      command_line_(command_line),
      history_disabled_pref_(history_disabled_pref),
      sync_service_url_(sync_service_url),
      ui_thread_(ui_thread),
      db_thread_(db_thread),
      token_service_(token_service),
      url_request_context_getter_(url_request_context_getter),
      web_data_service_(web_data_service),
      password_store_(password_store),
      weak_factory_(this) {
  DCHECK(token_service_);
  DCHECK(url_request_context_getter_);
}

ProfileSyncComponentsFactoryImpl::~ProfileSyncComponentsFactoryImpl() {}

void ProfileSyncComponentsFactoryImpl::RegisterDataTypes(
    syncer::SyncService* sync_service,
    const RegisterDataTypesMethod& register_platform_types_method) {
  syncer::ModelTypeSet disabled_types =
      GetDisabledTypesFromCommandLine(command_line_);
  syncer::ModelTypeSet enabled_types =
      GetEnabledTypesFromCommandLine(command_line_);
  RegisterCommonDataTypes(sync_service, disabled_types, enabled_types);
  if (!register_platform_types_method.is_null())
    register_platform_types_method.Run(sync_service, disabled_types,
                                       enabled_types);
}

void ProfileSyncComponentsFactoryImpl::RegisterCommonDataTypes(
    syncer::SyncService* sync_service,
    syncer::ModelTypeSet disabled_types,
    syncer::ModelTypeSet enabled_types) {
  base::Closure error_callback =
      base::Bind(&syncer::ReportUnrecoverableError, channel_);

  // TODO(stanisc): can DEVICE_INFO be one of disabled datatypes?
  if (FeatureList::IsEnabled(switches::kSyncUSSDeviceInfo)) {
    // Use an error callback that always uploads a stacktrace if it can to help
    // get USS as stable as possible.
    sync_service->RegisterDataTypeController(
        base::MakeUnique<ModelTypeController>(syncer::DEVICE_INFO, sync_client_,
                                              ui_thread_));
  } else {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<DeviceInfoDataTypeController>(
            error_callback, sync_client_,
            sync_service->GetLocalDeviceInfoProvider()));
  }

  // Autocomplete sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::AUTOFILL)) {
    if (FeatureList::IsEnabled(switches::kSyncUSSAutocomplete)) {
      sync_service->RegisterDataTypeController(
          base::MakeUnique<autofill::WebDataModelTypeController>(
              syncer::AUTOFILL, sync_client_, db_thread_, web_data_service_,
              base::Bind(
                  &autofill::AutocompleteSyncBridge::FromWebDataService)));
    } else {
      sync_service->RegisterDataTypeController(
          base::MakeUnique<AutofillDataTypeController>(
              db_thread_, error_callback, sync_client_, web_data_service_));
    }
  }

  // Autofill sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::AUTOFILL_PROFILE)) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<AutofillProfileDataTypeController>(
            db_thread_, error_callback, sync_client_, web_data_service_));
  }

  // Wallet data sync is enabled by default, but behind a syncer experiment
  // enforced by the datatype controller. Register unless explicitly disabled.
  bool wallet_disabled = disabled_types.Has(syncer::AUTOFILL_WALLET_DATA);
  if (!wallet_disabled) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<AutofillWalletDataTypeController>(
            syncer::AUTOFILL_WALLET_DATA, db_thread_, error_callback,
            sync_client_, web_data_service_));
  }

  // Wallet metadata sync depends on Wallet data sync. Register if Wallet data
  // is syncing and metadata sync is not explicitly disabled.
  if (!wallet_disabled &&
      !disabled_types.Has(syncer::AUTOFILL_WALLET_METADATA)) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<AutofillWalletDataTypeController>(
            syncer::AUTOFILL_WALLET_METADATA, db_thread_, error_callback,
            sync_client_, web_data_service_));
  }

  // Bookmark sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::BOOKMARKS)) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<BookmarkDataTypeController>(error_callback,
                                                     sync_client_));
  }

  const bool history_disabled =
      sync_client_->GetPrefService()->GetBoolean(history_disabled_pref_);
  // TypedUrl sync is enabled by default.  Register unless explicitly disabled,
  // or if saving history is disabled.
  if (!disabled_types.Has(syncer::TYPED_URLS) && !history_disabled) {
    if (base::FeatureList::IsEnabled(switches::kSyncUSSTypedURL)) {
      // TODO(gangwu): Register controller here once typed url controller
      // implemented.
    } else {
      sync_service->RegisterDataTypeController(
          base::MakeUnique<TypedUrlDataTypeController>(
              error_callback, sync_client_, history_disabled_pref_));
    }
  }

  // Delete directive sync is enabled by default.  Register unless full history
  // sync is disabled.
  if (!disabled_types.Has(syncer::HISTORY_DELETE_DIRECTIVES) &&
      !history_disabled) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<HistoryDeleteDirectivesDataTypeController>(
            error_callback, sync_client_));
  }

  // Session sync is enabled by default.  Register unless explicitly disabled.
  // This is also disabled if the browser history is disabled, because the
  // tab sync data is added to the web history on the server.
  if (!disabled_types.Has(syncer::PROXY_TABS) && !history_disabled) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<ProxyDataTypeController>(syncer::PROXY_TABS));
    sync_service->RegisterDataTypeController(
        base::MakeUnique<SessionDataTypeController>(
            error_callback, sync_client_,
            sync_service->GetLocalDeviceInfoProvider(),
            history_disabled_pref_));
  }

  // Favicon sync is enabled by default. Register unless explicitly disabled.
  if (!disabled_types.Has(syncer::FAVICON_IMAGES) &&
      !disabled_types.Has(syncer::FAVICON_TRACKING) && !history_disabled) {
    // crbug/384552. We disable error uploading for this data types for now.
    sync_service->RegisterDataTypeController(
        base::MakeUnique<AsyncDirectoryTypeController>(
            syncer::FAVICON_IMAGES, base::Closure(), sync_client_,
            syncer::GROUP_UI, ui_thread_));
    sync_service->RegisterDataTypeController(
        base::MakeUnique<AsyncDirectoryTypeController>(
            syncer::FAVICON_TRACKING, base::Closure(), sync_client_,
            syncer::GROUP_UI, ui_thread_));
  }

  // Password sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::PASSWORDS)) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<PasswordDataTypeController>(
            error_callback, sync_client_,
            sync_client_->GetPasswordStateChangedCallback(), password_store_));
  }

  if (!disabled_types.Has(syncer::PREFERENCES)) {
    if (!override_prefs_controller_to_uss_for_test_) {
      sync_service->RegisterDataTypeController(
          base::MakeUnique<AsyncDirectoryTypeController>(
              syncer::PREFERENCES, error_callback, sync_client_,
              syncer::GROUP_UI, ui_thread_));
    } else {
      sync_service->RegisterDataTypeController(
          base::MakeUnique<ModelTypeController>(syncer::PREFERENCES,
                                                sync_client_, ui_thread_));
    }
  }

  if (!disabled_types.Has(syncer::PRIORITY_PREFERENCES)) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<AsyncDirectoryTypeController>(
            syncer::PRIORITY_PREFERENCES, error_callback, sync_client_,
            syncer::GROUP_UI, ui_thread_));
  }

  // Article sync is disabled by default.  Register only if explicitly enabled.
  if (dom_distiller::IsEnableSyncArticlesSet()) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<AsyncDirectoryTypeController>(
            syncer::ARTICLES, error_callback, sync_client_, syncer::GROUP_UI,
            ui_thread_));
  }

#if defined(OS_CHROMEOS)
  if (!disabled_types.Has(syncer::PRINTERS)) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<ModelTypeController>(syncer::PRINTERS, sync_client_,
                                              ui_thread_));
  }
#endif

  // Reading list sync is enabled by default only on iOS. Register unless
  // Reading List or Reading List Sync is explicitly disabled.
  if (!disabled_types.Has(syncer::READING_LIST) &&
      reading_list::switches::IsReadingListEnabled()) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<ModelTypeController>(syncer::READING_LIST,
                                              sync_client_, ui_thread_));
  }

  if (!disabled_types.Has(syncer::USER_EVENTS) &&
      FeatureList::IsEnabled(switches::kSyncUserEvents)) {
    sync_service->RegisterDataTypeController(
        base::MakeUnique<ModelTypeController>(syncer::USER_EVENTS, sync_client_,
                                              ui_thread_));
  }
}

DataTypeManager* ProfileSyncComponentsFactoryImpl::CreateDataTypeManager(
    syncer::ModelTypeSet initial_types,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const DataTypeController::TypeMap* controllers,
    const syncer::DataTypeEncryptionHandler* encryption_handler,
    syncer::ModelTypeConfigurer* configurer,
    DataTypeManagerObserver* observer) {
  return new DataTypeManagerImpl(sync_client_, initial_types,
                                 debug_info_listener, controllers,
                                 encryption_handler, configurer, observer);
}

syncer::SyncEngine* ProfileSyncComponentsFactoryImpl::CreateSyncEngine(
    const std::string& name,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<syncer::SyncPrefs>& sync_prefs,
    const base::FilePath& sync_data_folder) {
  return new syncer::SyncBackendHostImpl(name, sync_client_, invalidator,
                                         sync_prefs, sync_data_folder);
}

std::unique_ptr<syncer::LocalDeviceInfoProvider>
ProfileSyncComponentsFactoryImpl::CreateLocalDeviceInfoProvider() {
  return base::MakeUnique<syncer::LocalDeviceInfoProviderImpl>(
      channel_, version_, is_tablet_);
}

class TokenServiceProvider
    : public OAuth2TokenServiceRequest::TokenServiceProvider {
 public:
  TokenServiceProvider(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      OAuth2TokenService* token_service);

  // OAuth2TokenServiceRequest::TokenServiceProvider implementation.
  scoped_refptr<base::SingleThreadTaskRunner> GetTokenServiceTaskRunner()
      override;
  OAuth2TokenService* GetTokenService() override;

 private:
  ~TokenServiceProvider() override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  OAuth2TokenService* token_service_;
};

TokenServiceProvider::TokenServiceProvider(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    OAuth2TokenService* token_service)
    : task_runner_(task_runner), token_service_(token_service) {}

TokenServiceProvider::~TokenServiceProvider() {}

scoped_refptr<base::SingleThreadTaskRunner>
TokenServiceProvider::GetTokenServiceTaskRunner() {
  return task_runner_;
}

OAuth2TokenService* TokenServiceProvider::GetTokenService() {
  return token_service_;
}

std::unique_ptr<syncer::AttachmentService>
ProfileSyncComponentsFactoryImpl::CreateAttachmentService(
    std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store,
    const syncer::UserShare& user_share,
    const std::string& store_birthday,
    syncer::ModelType model_type,
    syncer::AttachmentService::Delegate* delegate) {
  std::unique_ptr<syncer::AttachmentUploader> attachment_uploader;
  std::unique_ptr<syncer::AttachmentDownloader> attachment_downloader;
  // Only construct an AttachmentUploader and AttachmentDownload if we have sync
  // credentials. We may not have sync credentials because there may not be a
  // signed in sync user.
  if (!user_share.sync_credentials.account_id.empty() &&
      !user_share.sync_credentials.scope_set.empty()) {
    scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>
        token_service_provider(
            new TokenServiceProvider(ui_thread_, token_service_));
    // TODO(maniscalco): Use shared (one per profile) thread-safe instances of
    // AttachmentUploader and AttachmentDownloader instead of creating a new one
    // per AttachmentService (bug 369536).
    attachment_uploader = syncer::AttachmentUploader::Create(
        sync_service_url_, url_request_context_getter_,
        user_share.sync_credentials.account_id,
        user_share.sync_credentials.scope_set, token_service_provider,
        store_birthday, model_type);

    token_service_provider =
        new TokenServiceProvider(ui_thread_, token_service_);
    attachment_downloader = syncer::AttachmentDownloader::Create(
        sync_service_url_, url_request_context_getter_,
        user_share.sync_credentials.account_id,
        user_share.sync_credentials.scope_set, token_service_provider,
        store_birthday, model_type);
  }

  // It is important that the initial backoff delay is relatively large.  For
  // whatever reason, the server may fail all requests for a short period of
  // time.  When this happens we don't want to overwhelm the server with
  // requests so we use a large initial backoff.
  const base::TimeDelta initial_backoff_delay =
      base::TimeDelta::FromMinutes(30);
  const base::TimeDelta max_backoff_delay = base::TimeDelta::FromHours(4);
  return syncer::AttachmentService::Create(
      std::move(attachment_store), std::move(attachment_uploader),
      std::move(attachment_downloader), delegate, initial_backoff_delay,
      max_backoff_delay);
}

syncer::SyncApiComponentFactory::SyncComponents
ProfileSyncComponentsFactoryImpl::CreateBookmarkSyncComponents(
    syncer::SyncService* sync_service,
    std::unique_ptr<syncer::DataTypeErrorHandler> error_handler) {
  BookmarkModel* bookmark_model =
      sync_service->GetSyncClient()->GetBookmarkModel();
  syncer::UserShare* user_share = sync_service->GetUserShare();
// TODO(akalin): We may want to propagate this switch up eventually.
#if defined(OS_ANDROID) || defined(OS_IOS)
  const bool kExpectMobileBookmarksFolder = true;
#else
  const bool kExpectMobileBookmarksFolder = false;
#endif
  BookmarkModelAssociator* model_associator = new BookmarkModelAssociator(
      bookmark_model, sync_service->GetSyncClient(), user_share,
      error_handler->Copy(), kExpectMobileBookmarksFolder);
  BookmarkChangeProcessor* change_processor =
      new BookmarkChangeProcessor(sync_service->GetSyncClient(),
                                  model_associator, std::move(error_handler));
  return SyncComponents(model_associator, change_processor);
}

// static
void ProfileSyncComponentsFactoryImpl::OverridePrefsForUssTest(bool use_uss) {
  override_prefs_controller_to_uss_for_test_ = use_uss;
}

bool ProfileSyncComponentsFactoryImpl::
    override_prefs_controller_to_uss_for_test_ = false;

}  // namespace browser_sync
