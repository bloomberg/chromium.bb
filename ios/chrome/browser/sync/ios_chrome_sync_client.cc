// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_chrome_sync_client.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
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
#include "components/keyed_service/core/service_access_type.h"
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
#include "components/sync_sessions/favicon_cache.h"
#include "components/sync_sessions/local_session_event_router.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/invalidation/ios_chrome_profile_invalidation_provider_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/sync/glue/sync_start_util.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sessions/ios_chrome_local_session_event_router.h"
#include "ios/chrome/browser/undo/bookmark_undo_service_factory.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/web_thread.h"
#include "sync/internal_api/public/engine/passive_model_worker.h"
#include "sync/util/extensions_activity.h"
#include "ui/base/device_form_factor.h"

namespace {

// iOS implementation of SyncSessionsClient. Needs to be in a separate class
// due to possible multiple inheritance issues, wherein IOSChromeSyncClient
// might inherit from other interfaces with same methods.
class SyncSessionsClientImpl : public sync_sessions::SyncSessionsClient {
 public:
  explicit SyncSessionsClientImpl(ios::ChromeBrowserState* browser_state)
      : browser_state_(browser_state),
        window_delegates_getter_(
            ios::GetChromeBrowserProvider()
                ->CreateSyncedWindowDelegatesGetter(browser_state)) {}

  ~SyncSessionsClientImpl() override {}

  // SyncSessionsClient implementation.
  bookmarks::BookmarkModel* GetBookmarkModel() override {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
    return ios::BookmarkModelFactory::GetForBrowserState(browser_state_);
  }

  favicon::FaviconService* GetFaviconService() override {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
    return ios::FaviconServiceFactory::GetForBrowserState(
        browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
  }

  history::HistoryService* GetHistoryService() override {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
    return ios::HistoryServiceFactory::GetForBrowserState(
        browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
  }

  bool ShouldSyncURL(const GURL& url) const override {
    if (url == GURL(kChromeUIHistoryURL)) {
      // The history page is treated specially as we want it to trigger syncable
      // events for UI purposes.
      return true;
    }
    return url.is_valid() && !url.SchemeIs(kChromeUIScheme) &&
           !url.SchemeIsFile();
  }

  browser_sync::SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter()
      override {
    return window_delegates_getter_.get();
  }

  scoped_ptr<browser_sync::LocalSessionEventRouter> GetLocalSessionEventRouter()
      override {
    syncer::SyncableService::StartSyncFlare flare(
        ios::sync_start_util::GetFlareForSyncableService(
            browser_state_->GetStatePath()));
    return make_scoped_ptr(
        new IOSChromeLocalSessionEventRouter(browser_state_, this, flare));
  }

 private:
  ios::ChromeBrowserState* const browser_state_;
  const scoped_ptr<browser_sync::SyncedWindowDelegatesGetter>
      window_delegates_getter_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionsClientImpl);
};

}  // namespace

IOSChromeSyncClient::IOSChromeSyncClient(ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      sync_sessions_client_(new SyncSessionsClientImpl(browser_state)),
      dummy_extensions_activity_(new syncer::ExtensionsActivity()),
      weak_ptr_factory_(this) {}

IOSChromeSyncClient::~IOSChromeSyncClient() {}

void IOSChromeSyncClient::Initialize() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);

  web_data_service_ =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
  // TODO(crbug.com/558320) Is EXPLICIT_ACCESS appropriate here?
  password_store_ = IOSChromePasswordStoreFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);

  // Component factory may already be set in tests.
  if (!GetSyncApiComponentFactory()) {
    const GURL sync_service_url = GetSyncServiceURL(
        *base::CommandLine::ForCurrentProcess(), ::GetChannel());
    ProfileOAuth2TokenService* token_service =
        OAuth2TokenServiceFactory::GetForBrowserState(browser_state_);

    net::URLRequestContextGetter* url_request_context_getter =
        browser_state_->GetRequestContext();

    component_factory_.reset(new ProfileSyncComponentsFactoryImpl(
        this, ::GetChannel(), ::GetVersionString(),
        ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET,
        *base::CommandLine::ForCurrentProcess(),
        prefs::kSavingBrowserHistoryDisabled, sync_service_url,
        web::WebThread::GetTaskRunnerForThread(web::WebThread::UI),
        web::WebThread::GetTaskRunnerForThread(web::WebThread::DB),
        token_service, url_request_context_getter, web_data_service_,
        password_store_));
  }
}

sync_driver::SyncService* IOSChromeSyncClient::GetSyncService() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  return IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state_);
}

PrefService* IOSChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  return browser_state_->GetPrefs();
}

bookmarks::BookmarkModel* IOSChromeSyncClient::GetBookmarkModel() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  return ios::BookmarkModelFactory::GetForBrowserState(browser_state_);
}

favicon::FaviconService* IOSChromeSyncClient::GetFaviconService() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  return ios::FaviconServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
}

history::HistoryService* IOSChromeSyncClient::GetHistoryService() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  return ios::HistoryServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
}

autofill::PersonalDataManager* IOSChromeSyncClient::GetPersonalDataManager() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  return PersonalDataManagerFactory::GetForBrowserState(browser_state_);
}

sync_driver::ClearBrowsingDataCallback
IOSChromeSyncClient::GetClearBrowsingDataCallback() {
  return base::Bind(&IOSChromeSyncClient::ClearBrowsingData,
                    base::Unretained(this));
}

base::Closure IOSChromeSyncClient::GetPasswordStateChangedCallback() {
  return base::Bind(
      &IOSChromePasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged,
      base::Unretained(browser_state_));
}

sync_driver::SyncApiComponentFactory::RegisterDataTypesMethod
IOSChromeSyncClient::GetRegisterPlatformTypesCallback() {
  // The iOS port does not have any platform-specific datatypes.
  return sync_driver::SyncApiComponentFactory::RegisterDataTypesMethod();
}

BookmarkUndoService* IOSChromeSyncClient::GetBookmarkUndoServiceIfExists() {
  return ios::BookmarkUndoServiceFactory::GetForBrowserStateIfExists(
      browser_state_);
}

invalidation::InvalidationService*
IOSChromeSyncClient::GetInvalidationService() {
  invalidation::ProfileInvalidationProvider* provider =
      IOSChromeProfileInvalidationProviderFactory::GetForBrowserState(
          browser_state_);
  if (provider)
    return provider->GetInvalidationService();
  return nullptr;
}

scoped_refptr<syncer::ExtensionsActivity>
IOSChromeSyncClient::GetExtensionsActivity() {
  // TODO(crbug.com/562048) Get rid of dummy_extensions_activity_ and return
  // nullptr.
  return dummy_extensions_activity_;
}

sync_sessions::SyncSessionsClient*
IOSChromeSyncClient::GetSyncSessionsClient() {
  return sync_sessions_client_.get();
}

base::WeakPtr<syncer::SyncableService>
IOSChromeSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  switch (type) {
    case syncer::DEVICE_INFO:
      return IOSChromeProfileSyncServiceFactory::GetForBrowserState(
                 browser_state_)
          ->GetDeviceInfoSyncableService()
          ->AsWeakPtr();
    case syncer::PREFERENCES:
      return browser_state_->GetSyncablePrefs()
          ->GetSyncableService(syncer::PREFERENCES)
          ->AsWeakPtr();
    case syncer::PRIORITY_PREFERENCES:
      return browser_state_->GetSyncablePrefs()
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
                   web_data_service_.get())
            ->AsWeakPtr();
      } else if (type == syncer::AUTOFILL_PROFILE) {
        return autofill::AutofillProfileSyncableService::FromWebDataService(
                   web_data_service_.get())
            ->AsWeakPtr();
      } else if (type == syncer::AUTOFILL_WALLET_METADATA) {
        return autofill::AutofillWalletMetadataSyncableService::
            FromWebDataService(web_data_service_.get())
                ->AsWeakPtr();
      }
      return autofill::AutofillWalletSyncableService::FromWebDataService(
                 web_data_service_.get())
          ->AsWeakPtr();
    }
    case syncer::HISTORY_DELETE_DIRECTIVES: {
      history::HistoryService* history =
          ios::HistoryServiceFactory::GetForBrowserState(
              browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
      return history ? history->AsWeakPtr()
                     : base::WeakPtr<history::HistoryService>();
    }
    case syncer::TYPED_URLS: {
      history::HistoryService* history =
          ios::HistoryServiceFactory::GetForBrowserState(
              browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
      return history ? history->GetTypedUrlSyncableService()->AsWeakPtr()
                     : base::WeakPtr<syncer::SyncableService>();
    }
    case syncer::FAVICON_IMAGES:
    case syncer::FAVICON_TRACKING: {
      browser_sync::FaviconCache* favicons =
          IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state_)
              ->GetFaviconCache();
      return favicons ? favicons->AsWeakPtr()
                      : base::WeakPtr<syncer::SyncableService>();
    }
    case syncer::ARTICLES: {
      dom_distiller::DomDistillerService* service =
          dom_distiller::DomDistillerServiceFactory::GetForBrowserState(
              browser_state_);
      if (service)
        return service->GetSyncableService()->AsWeakPtr();
      return base::WeakPtr<syncer::SyncableService>();
    }
    case syncer::SESSIONS: {
      return IOSChromeProfileSyncServiceFactory::GetForBrowserState(
                 browser_state_)
          ->GetSessionsSyncableService()
          ->AsWeakPtr();
    }
    case syncer::PASSWORDS: {
      return password_store_ ? password_store_->GetPasswordSyncableService()
                             : base::WeakPtr<syncer::SyncableService>();
    }
    default:
      NOTREACHED();
      return base::WeakPtr<syncer::SyncableService>();
  }
}

scoped_refptr<syncer::ModelSafeWorker>
IOSChromeSyncClient::CreateModelWorkerForGroup(
    syncer::ModelSafeGroup group,
    syncer::WorkerLoopDestructionObserver* observer) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  switch (group) {
    case syncer::GROUP_DB:
      return new browser_sync::BrowserThreadModelWorker(
          web::WebThread::GetTaskRunnerForThread(web::WebThread::DB),
          syncer::GROUP_DB, observer);
    case syncer::GROUP_FILE:
      return new browser_sync::BrowserThreadModelWorker(
          web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE),
          syncer::GROUP_FILE, observer);
    case syncer::GROUP_UI:
      return new browser_sync::UIModelWorker(
          web::WebThread::GetTaskRunnerForThread(web::WebThread::UI), observer);
    case syncer::GROUP_PASSIVE:
      return new syncer::PassiveModelWorker(observer);
    case syncer::GROUP_HISTORY: {
      history::HistoryService* history_service = GetHistoryService();
      if (!history_service)
        return nullptr;
      return new browser_sync::HistoryModelWorker(
          history_service->AsWeakPtr(),
          web::WebThread::GetTaskRunnerForThread(web::WebThread::UI), observer);
    }
    case syncer::GROUP_PASSWORD: {
      if (!password_store_)
        return nullptr;
      return new browser_sync::PasswordModelWorker(password_store_, observer);
    }
    default:
      return nullptr;
  }
}

sync_driver::SyncApiComponentFactory*
IOSChromeSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

void IOSChromeSyncClient::ClearBrowsingData(base::Time start, base::Time end) {
  // This method should never be called on iOS.
  NOTREACHED();
}

void IOSChromeSyncClient::SetSyncApiComponentFactoryForTesting(
    scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory) {
  component_factory_ = std::move(component_factory);
}
