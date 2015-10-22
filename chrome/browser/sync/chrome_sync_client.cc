// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/chrome_sync_client.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/history/core/browser/history_model_worker.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/sync/browser/password_model_worker.h"
#include "components/sync_driver/glue/browser_thread_model_worker.h"
#include "components/sync_driver/glue/ui_model_worker.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "sync/internal_api/public/engine/passive_model_worker.h"

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

#if defined(OS_CHROMEOS)
#include "components/wifi_sync/wifi_credential_syncable_service.h"
#include "components/wifi_sync/wifi_credential_syncable_service_factory.h"
#endif

using content::BrowserThread;

namespace browser_sync {

ChromeSyncClient::ChromeSyncClient(
    Profile* profile,
    scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory)
    : profile_(profile), component_factory_(component_factory.Pass()) {}
ChromeSyncClient::~ChromeSyncClient() {
}

void ChromeSyncClient::Initialize(sync_driver::SyncService* sync_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_service_ = sync_service;
  web_data_service_ = GetWebDataService();
  password_store_ = GetPasswordStore();
  component_factory_->RegisterDataTypes(this);
}

sync_driver::SyncService* ChromeSyncClient::GetSyncService() {
  // TODO(zea): bring back this DCHECK after Typed URLs are converted to
  // SyncableService.
  // DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sync_service_;
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
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
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

scoped_refptr<password_manager::PasswordStore>
ChromeSyncClient::GetPasswordStore() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return PasswordStoreFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

scoped_refptr<autofill::AutofillWebDataService>
ChromeSyncClient::GetWebDataService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return WebDataServiceFactory::GetAutofillWebDataForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

BookmarkUndoService* ChromeSyncClient::GetBookmarkUndoServiceIfExists() {
  return BookmarkUndoServiceFactory::GetForProfileIfExists(profile_);
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
      scoped_refptr<password_manager::PasswordStore> password_store =
          GetPasswordStore();
      if (!password_store.get())
        return nullptr;
      return new PasswordModelWorker(password_store, observer);
    }
    default:
      return nullptr;
  }
}

sync_driver::SyncApiComponentFactory*
ChromeSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

}  // namespace browser_sync
