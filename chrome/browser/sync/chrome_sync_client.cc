// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/chrome_sync_client.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/sync/glue/theme_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sessions/notification_service_sessions_router.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browser_sync/browser/profile_sync_components_factory_impl.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/browser_sync/common/browser_sync_switches.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/history/core/browser/history_model_worker.h"
#include "components/history/core/browser/history_service.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/sync/browser/password_model_worker.h"
#include "components/search_engines/search_engine_data_type_controller.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync_driver/glue/browser_thread_model_worker.h"
#include "components/sync_driver/glue/chrome_report_unrecoverable_error.h"
#include "components/sync_driver/glue/ui_model_worker.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "components/sync_driver/sync_util.h"
#include "components/sync_driver/ui_data_type_controller.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "sync/internal_api/public/engine/passive_model_worker.h"
#include "ui/base/device_form_factor.h"

#if defined(ENABLE_APP_LIST)
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "ui/app_list/app_list_switches.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/sync/glue/extension_data_type_controller.h"
#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_sync_data_type_controller.h"
#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"
#endif

#if defined(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/sync/glue/synced_window_delegates_getter_android.h"
#endif

#if defined(OS_CHROMEOS)
#include "components/wifi_sync/wifi_credential_syncable_service.h"
#include "components/wifi_sync/wifi_credential_syncable_service_factory.h"
#endif

using content::BrowserThread;
#if defined(ENABLE_EXTENSIONS)
using browser_sync::ExtensionDataTypeController;
using browser_sync::ExtensionSettingDataTypeController;
#endif
using browser_sync::SearchEngineDataTypeController;
using sync_driver::UIDataTypeController;

namespace browser_sync {

// Chrome implementation of SyncSessionsClient. Needs to be in a separate class
// due to possible multiple inheritance issues, wherein ChromeSyncClient might
// inherit from other interfaces with same methods.
class SyncSessionsClientImpl : public sync_sessions::SyncSessionsClient {
 public:
  explicit SyncSessionsClientImpl(Profile* profile) : profile_(profile) {
    window_delegates_getter_.reset(
#if BUILDFLAG(ANDROID_JAVA_UI)
        // Android doesn't have multi-profile support, so no need to pass the
        // profile in.
        new browser_sync::SyncedWindowDelegatesGetterAndroid());
#else
        new browser_sync::BrowserSyncedWindowDelegatesGetter(profile));
#endif
  }
  ~SyncSessionsClientImpl() override {}

  // SyncSessionsClient implementation.
  bookmarks::BookmarkModel* GetBookmarkModel() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return BookmarkModelFactory::GetForProfile(profile_);
  }
  favicon::FaviconService* GetFaviconService() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return FaviconServiceFactory::GetForProfile(
        profile_, ServiceAccessType::IMPLICIT_ACCESS);
  }
  history::HistoryService* GetHistoryService() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return HistoryServiceFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);
  }
  bool ShouldSyncURL(const GURL& url) const override {
    if (url == GURL(chrome::kChromeUIHistoryURL)) {
      // The history page is treated specially as we want it to trigger syncable
      // events for UI purposes.
      return true;
    }
    return url.is_valid() && !url.SchemeIs(content::kChromeUIScheme) &&
           !url.SchemeIs(chrome::kChromeNativeScheme) && !url.SchemeIsFile();
  }

  SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter() override {
    return window_delegates_getter_.get();
  }

  scoped_ptr<browser_sync::LocalSessionEventRouter> GetLocalSessionEventRouter()
      override {
    syncer::SyncableService::StartSyncFlare flare(
        sync_start_util::GetFlareForSyncableService(profile_->GetPath()));
    return make_scoped_ptr(
        new NotificationServiceSessionsRouter(profile_, this, flare));
  }

 private:
  Profile* profile_;
  scoped_ptr<SyncedWindowDelegatesGetter> window_delegates_getter_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionsClientImpl);
};

ChromeSyncClient::ChromeSyncClient(Profile* profile)
    : profile_(profile),
      sync_sessions_client_(new SyncSessionsClientImpl(profile)),
      browsing_data_remover_observer_(NULL),
      weak_ptr_factory_(this) {}

ChromeSyncClient::~ChromeSyncClient() {
}

void ChromeSyncClient::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_data_service_ = WebDataServiceFactory::GetAutofillWebDataForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
  password_store_ = PasswordStoreFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);

  // Component factory may already be set in tests.
  if (!GetSyncApiComponentFactory()) {
    const GURL sync_service_url = GetSyncServiceURL(
        *base::CommandLine::ForCurrentProcess(), chrome::GetChannel());
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
    net::URLRequestContextGetter* url_request_context_getter =
        profile_->GetRequestContext();

    component_factory_.reset(new ProfileSyncComponentsFactoryImpl(
        this, chrome::GetChannel(), chrome::GetVersionString(),
        ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET,
        *base::CommandLine::ForCurrentProcess(),
        prefs::kSavingBrowserHistoryDisabled, sync_service_url,
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::UI),
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::DB),
        token_service, url_request_context_getter, web_data_service_,
        password_store_));
  }
}

sync_driver::SyncService* ChromeSyncClient::GetSyncService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(profile_);
}

PrefService* ChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return profile_->GetPrefs();
}

bookmarks::BookmarkModel* ChromeSyncClient::GetBookmarkModel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BookmarkModelFactory::GetForProfile(profile_);
}

favicon::FaviconService* ChromeSyncClient::GetFaviconService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
}

history::HistoryService* ChromeSyncClient::GetHistoryService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

autofill::PersonalDataManager* ChromeSyncClient::GetPersonalDataManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return autofill::PersonalDataManagerFactory::GetForProfile(profile_);
}

sync_driver::ClearBrowsingDataCallback
ChromeSyncClient::GetClearBrowsingDataCallback() {
  return base::Bind(&ChromeSyncClient::ClearBrowsingData,
                    base::Unretained(this));
}

base::Closure ChromeSyncClient::GetPasswordStateChangedCallback() {
  return base::Bind(
      &PasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged,
      base::Unretained(profile_));
}

sync_driver::SyncApiComponentFactory::RegisterDataTypesMethod
ChromeSyncClient::GetRegisterPlatformTypesCallback() {
  return base::Bind(
#if BUILDFLAG(ANDROID_JAVA_UI)
      &ChromeSyncClient::RegisterAndroidDataTypes,
#else
      &ChromeSyncClient::RegisterDesktopDataTypes,
#endif  // BUILDFLAG(ANDROID_JAVA_UI)
      weak_ptr_factory_.GetWeakPtr());
}

BookmarkUndoService* ChromeSyncClient::GetBookmarkUndoServiceIfExists() {
  return BookmarkUndoServiceFactory::GetForProfileIfExists(profile_);
}

invalidation::InvalidationService* ChromeSyncClient::GetInvalidationService() {
  invalidation::ProfileInvalidationProvider* provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile_);
  if (provider)
    return provider->GetInvalidationService();
  return nullptr;
}

scoped_refptr<syncer::ExtensionsActivity>
ChromeSyncClient::GetExtensionsActivity() {
  return extensions_activity_monitor_.GetExtensionsActivity();
}

sync_sessions::SyncSessionsClient* ChromeSyncClient::GetSyncSessionsClient() {
  return sync_sessions_client_.get();
}

base::WeakPtr<syncer::SyncableService>
ChromeSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  if (!profile_) {  // For tests.
     return base::WeakPtr<syncer::SyncableService>();
  }
  switch (type) {
    case syncer::DEVICE_INFO:
      return ProfileSyncServiceFactory::GetForProfile(profile_)
          ->GetDeviceInfoSyncableService()
          ->AsWeakPtr();
    case syncer::PREFERENCES:
      return PrefServiceSyncableFromProfile(profile_)
          ->GetSyncableService(syncer::PREFERENCES)
          ->AsWeakPtr();
    case syncer::PRIORITY_PREFERENCES:
      return PrefServiceSyncableFromProfile(profile_)
          ->GetSyncableService(syncer::PRIORITY_PREFERENCES)
          ->AsWeakPtr();
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_PROFILE:
    case syncer::AUTOFILL_WALLET_DATA:
    case syncer::AUTOFILL_WALLET_METADATA: {
      if (!web_data_service_)
        return base::WeakPtr<syncer::SyncableService>();
      if (type == syncer::AUTOFILL) {
        return autofill::AutocompleteSyncableService::FromWebDataService(
            web_data_service_.get())->AsWeakPtr();
      } else if (type == syncer::AUTOFILL_PROFILE) {
        return autofill::AutofillProfileSyncableService::FromWebDataService(
            web_data_service_.get())->AsWeakPtr();
      } else if (type == syncer::AUTOFILL_WALLET_METADATA) {
        return autofill::AutofillWalletMetadataSyncableService::
            FromWebDataService(web_data_service_.get())->AsWeakPtr();
      }
      return autofill::AutofillWalletSyncableService::FromWebDataService(
          web_data_service_.get())->AsWeakPtr();
    }
    case syncer::SEARCH_ENGINES:
      return TemplateURLServiceFactory::GetForProfile(profile_)->AsWeakPtr();
#if defined(ENABLE_EXTENSIONS)
    case syncer::APPS:
    case syncer::EXTENSIONS:
      return ExtensionSyncService::Get(profile_)->AsWeakPtr();
    case syncer::APP_SETTINGS:
    case syncer::EXTENSION_SETTINGS:
      return extensions::settings_sync_util::GetSyncableService(profile_, type)
          ->AsWeakPtr();
#endif
#if defined(ENABLE_APP_LIST)
    case syncer::APP_LIST:
      return app_list::AppListSyncableServiceFactory::GetForProfile(profile_)->
          AsWeakPtr();
#endif
#if defined(ENABLE_THEMES)
    case syncer::THEMES:
      return ThemeServiceFactory::GetForProfile(profile_)->
          GetThemeSyncableService()->AsWeakPtr();
#endif
    case syncer::HISTORY_DELETE_DIRECTIVES: {
      history::HistoryService* history = GetHistoryService();
      return history ? history->AsWeakPtr()
                     : base::WeakPtr<history::HistoryService>();
    }
    case syncer::TYPED_URLS: {
      // We request history service with explicit access here because this
      // codepath is executed on backend thread while HistoryServiceFactory
      // checks preference value in implicit mode and PrefService expectes calls
      // only from UI thread.
      history::HistoryService* history = HistoryServiceFactory::GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
      if (!history)
        return base::WeakPtr<history::TypedUrlSyncableService>();
      return history->GetTypedUrlSyncableService()->AsWeakPtr();
    }
#if defined(ENABLE_SPELLCHECK)
    case syncer::DICTIONARY:
      return SpellcheckServiceFactory::GetForContext(profile_)->
          GetCustomDictionary()->AsWeakPtr();
#endif
    case syncer::FAVICON_IMAGES:
    case syncer::FAVICON_TRACKING: {
      browser_sync::FaviconCache* favicons =
          ProfileSyncServiceFactory::GetForProfile(profile_)->
              GetFaviconCache();
      return favicons ? favicons->AsWeakPtr()
                      : base::WeakPtr<syncer::SyncableService>();
    }
#if defined(ENABLE_SUPERVISED_USERS)
    case syncer::SUPERVISED_USER_SETTINGS:
      return SupervisedUserSettingsServiceFactory::GetForProfile(profile_)->
          AsWeakPtr();
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    case syncer::SUPERVISED_USERS:
      return SupervisedUserSyncServiceFactory::GetForProfile(profile_)->
          AsWeakPtr();
    case syncer::SUPERVISED_USER_SHARED_SETTINGS:
      return SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
          profile_)->AsWeakPtr();
#endif
    case syncer::SUPERVISED_USER_WHITELISTS:
      return SupervisedUserServiceFactory::GetForProfile(profile_)
          ->GetWhitelistService()
          ->AsWeakPtr();
#endif
    case syncer::ARTICLES: {
      dom_distiller::DomDistillerService* service =
          dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
              profile_);
      if (service)
        return service->GetSyncableService()->AsWeakPtr();
      return base::WeakPtr<syncer::SyncableService>();
    }
    case syncer::SESSIONS: {
      return ProfileSyncServiceFactory::GetForProfile(profile_)->
          GetSessionsSyncableService()->AsWeakPtr();
    }
    case syncer::PASSWORDS: {
      return password_store_.get()
                 ? password_store_->GetPasswordSyncableService()
                 : base::WeakPtr<syncer::SyncableService>();
    }
#if defined(OS_CHROMEOS)
    case syncer::WIFI_CREDENTIALS:
      return wifi_sync::WifiCredentialSyncableServiceFactory::
          GetForBrowserContext(profile_)->AsWeakPtr();
#endif
    default:
      // The following datatypes still need to be transitioned to the
      // syncer::SyncableService API:
      // Bookmarks
      // Typed URLs
      NOTREACHED();
      return base::WeakPtr<syncer::SyncableService>();
  }
}

scoped_refptr<syncer::ModelSafeWorker>
ChromeSyncClient::CreateModelWorkerForGroup(
    syncer::ModelSafeGroup group,
    syncer::WorkerLoopDestructionObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (group) {
    case syncer::GROUP_DB:
      return new BrowserThreadModelWorker(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
          syncer::GROUP_DB, observer);
    case syncer::GROUP_FILE:
      return new BrowserThreadModelWorker(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          syncer::GROUP_FILE, observer);
    case syncer::GROUP_UI:
      return new UIModelWorker(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          observer);
    case syncer::GROUP_PASSIVE:
      return new syncer::PassiveModelWorker(observer);
    case syncer::GROUP_HISTORY: {
      history::HistoryService* history_service = GetHistoryService();
      if (!history_service)
        return nullptr;
      return new HistoryModelWorker(
          history_service->AsWeakPtr(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          observer);
    }
    case syncer::GROUP_PASSWORD: {
      if (!password_store_.get())
        return nullptr;
      return new PasswordModelWorker(password_store_, observer);
    }
    default:
      return nullptr;
  }
}

sync_driver::SyncApiComponentFactory*
ChromeSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

void ChromeSyncClient::ClearBrowsingData(base::Time start, base::Time end) {
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(profile_);
  remover->Remove(BrowsingDataRemover::TimeRange(start, end),
                  BrowsingDataRemover::REMOVE_ALL, BrowsingDataHelper::ALL);

  password_store_->RemoveLoginsSyncedBetween(start, end);
}

void ChromeSyncClient::SetBrowsingDataRemoverObserverForTesting(
    BrowsingDataRemover::Observer* observer) {
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(profile_);
  if (browsing_data_remover_observer_)
    remover->RemoveObserver(browsing_data_remover_observer_);

  if (observer)
    remover->AddObserver(observer);

  browsing_data_remover_observer_ = observer;
}

void ChromeSyncClient::SetSyncApiComponentFactoryForTesting(
    scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory) {
  component_factory_ = std::move(component_factory);
}

void ChromeSyncClient::RegisterDesktopDataTypes(
    sync_driver::SyncService* sync_service,
    syncer::ModelTypeSet disabled_types,
    syncer::ModelTypeSet enabled_types) {
  base::Closure error_callback =
      base::Bind(&ChromeReportUnrecoverableError, chrome::GetChannel());
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);

#if defined(ENABLE_EXTENSIONS)
  // App sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::APPS)) {
    sync_service->RegisterDataTypeController(new ExtensionDataTypeController(
        syncer::APPS, error_callback, this, profile_));
  }

  // Extension sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::EXTENSIONS)) {
    sync_service->RegisterDataTypeController(new ExtensionDataTypeController(
        syncer::EXTENSIONS, error_callback, this, profile_));
  }
#endif

  // Preference sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::PREFERENCES)) {
    sync_service->RegisterDataTypeController(new UIDataTypeController(
        ui_thread, error_callback, syncer::PREFERENCES, this));
  }

#if defined(ENABLE_THEMES)
  // Theme sync is enabled by default.  Register unless explicitly disabled.
  if (!disabled_types.Has(syncer::THEMES)) {
    sync_service->RegisterDataTypeController(
        new ThemeDataTypeController(error_callback, this, profile_));
  }
#endif

  // Search Engine sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::SEARCH_ENGINES)) {
    sync_service->RegisterDataTypeController(new SearchEngineDataTypeController(
        ui_thread, error_callback, this,
        TemplateURLServiceFactory::GetForProfile(profile_)));
  }

#if defined(ENABLE_EXTENSIONS)
  // Extension setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::EXTENSION_SETTINGS)) {
    sync_service->RegisterDataTypeController(
        new ExtensionSettingDataTypeController(syncer::EXTENSION_SETTINGS,
                                               error_callback, this, profile_));
  }

  // App setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::APP_SETTINGS)) {
    sync_service->RegisterDataTypeController(
        new ExtensionSettingDataTypeController(syncer::APP_SETTINGS,
                                               error_callback, this, profile_));
  }
#endif

#if defined(ENABLE_APP_LIST)
  if (app_list::switches::IsAppListSyncEnabled()) {
    sync_service->RegisterDataTypeController(new UIDataTypeController(
        ui_thread, error_callback, syncer::APP_LIST, this));
  }
#endif

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS)
  // Dictionary sync is enabled by default.
  if (!disabled_types.Has(syncer::DICTIONARY)) {
    sync_service->RegisterDataTypeController(new UIDataTypeController(
        ui_thread, error_callback, syncer::DICTIONARY, this));
  }
#endif

#if defined(ENABLE_SUPERVISED_USERS)
  sync_service->RegisterDataTypeController(
      new SupervisedUserSyncDataTypeController(syncer::SUPERVISED_USER_SETTINGS,
                                               error_callback, this, profile_));
  sync_service->RegisterDataTypeController(
      new SupervisedUserSyncDataTypeController(
          syncer::SUPERVISED_USER_WHITELISTS, error_callback, this, profile_));
  sync_service->RegisterDataTypeController(
      new SupervisedUserSyncDataTypeController(syncer::SUPERVISED_USERS,
                                               error_callback, this, profile_));
  sync_service->RegisterDataTypeController(
      new SupervisedUserSyncDataTypeController(
          syncer::SUPERVISED_USER_SHARED_SETTINGS, error_callback, this,
          profile_));
#endif

#if defined(OS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWifiCredentialSync) &&
      !disabled_types.Has(syncer::WIFI_CREDENTIALS)) {
    sync_service->RegisterDataTypeController(new UIDataTypeController(
        ui_thread, error_callback, syncer::WIFI_CREDENTIALS, this));
  }
#endif
}

void ChromeSyncClient::RegisterAndroidDataTypes(
    sync_driver::SyncService* sync_service,
    syncer::ModelTypeSet disabled_types,
    syncer::ModelTypeSet enabled_types) {
  base::Closure error_callback =
      base::Bind(&ChromeReportUnrecoverableError, chrome::GetChannel());
#if defined(ENABLE_SUPERVISED_USERS)
  sync_service->RegisterDataTypeController(
      new SupervisedUserSyncDataTypeController(syncer::SUPERVISED_USER_SETTINGS,
                                               error_callback, this, profile_));
  sync_service->RegisterDataTypeController(
      new SupervisedUserSyncDataTypeController(
          syncer::SUPERVISED_USER_WHITELISTS, error_callback, this, profile_));
#endif
}

}  // namespace browser_sync
