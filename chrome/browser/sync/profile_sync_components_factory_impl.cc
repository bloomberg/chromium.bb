// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/pref_service_flags_storage.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/extension_backed_data_type_controller.h"
#include "chrome/browser/sync/glue/extension_data_type_controller.h"
#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"
#include "chrome/browser/sync/glue/local_device_info_provider_impl.h"
#include "chrome/browser/sync/glue/password_data_type_controller.h"
#include "chrome/browser/sync/glue/search_engine_data_type_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/sync_backend_host_impl.h"
#include "chrome/browser/sync/glue/theme_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_change_processor.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sessions/session_data_type_controller.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/webdata/autocomplete_syncable_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/data_type_manager_impl.h"
#include "components/sync_driver/data_type_manager_observer.h"
#include "components/sync_driver/generic_change_processor.h"
#include "components/sync_driver/proxy_data_type_controller.h"
#include "components/sync_driver/shared_change_processor.h"
#include "components/sync_driver/ui_data_type_controller.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/oauth2_token_service_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/api/attachments/fake_attachment_store.h"
#include "sync/api/syncable_service.h"
#include "sync/internal_api/public/attachments/attachment_downloader.h"
#include "sync/internal_api/public/attachments/attachment_service.h"
#include "sync/internal_api/public/attachments/attachment_service_impl.h"
#include "sync/internal_api/public/attachments/attachment_uploader_impl.h"

#if defined(ENABLE_APP_LIST)
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "ui/app_list/app_list_switches.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/api/synced_notifications_private/synced_notifications_shim.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_sync_data_type_controller.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service_factory.h"
#endif

#if defined(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#endif

using browser_sync::AutofillDataTypeController;
using browser_sync::AutofillProfileDataTypeController;
using browser_sync::BookmarkChangeProcessor;
using browser_sync::BookmarkDataTypeController;
using browser_sync::BookmarkModelAssociator;
using browser_sync::ChromeReportUnrecoverableError;
using browser_sync::ExtensionBackedDataTypeController;
using browser_sync::ExtensionDataTypeController;
using browser_sync::ExtensionSettingDataTypeController;
using browser_sync::PasswordDataTypeController;
using browser_sync::SearchEngineDataTypeController;
using browser_sync::SessionDataTypeController;
using browser_sync::SyncBackendHost;
using browser_sync::ThemeDataTypeController;
using browser_sync::TypedUrlChangeProcessor;
using browser_sync::TypedUrlDataTypeController;
using browser_sync::TypedUrlModelAssociator;
using content::BrowserThread;
using sync_driver::DataTypeController;
using sync_driver::DataTypeErrorHandler;
using sync_driver::DataTypeManager;
using sync_driver::DataTypeManagerImpl;
using sync_driver::DataTypeManagerObserver;
using sync_driver::ProxyDataTypeController;
using sync_driver::SharedChangeProcessor;
using sync_driver::UIDataTypeController;

namespace {

syncer::ModelTypeSet GetDisabledTypesFromCommandLine(
    const CommandLine& command_line) {
  syncer::ModelTypeSet disabled_types;
  std::string disabled_types_str =
      command_line.GetSwitchValueASCII(switches::kDisableSyncTypes);
  disabled_types = syncer::ModelTypeSetFromString(disabled_types_str);
  return disabled_types;
}

syncer::ModelTypeSet GetEnabledTypesFromCommandLine(
    const CommandLine& command_line) {
  syncer::ModelTypeSet enabled_types;
  if (command_line.HasSwitch(switches::kEnableSyncSyncedNotifications)) {
    enabled_types.Put(syncer::SYNCED_NOTIFICATIONS);
  }
  return enabled_types;
}

}  // namespace

ProfileSyncComponentsFactoryImpl::ProfileSyncComponentsFactoryImpl(
    Profile* profile,
    CommandLine* command_line,
    const GURL& sync_service_url,
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* url_request_context_getter)
    : profile_(profile),
      command_line_(command_line),
      web_data_service_(WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, Profile::EXPLICIT_ACCESS)),
      sync_service_url_(sync_service_url),
      token_service_(token_service),
      url_request_context_getter_(url_request_context_getter),
      weak_factory_(this) {
  DCHECK(token_service_);
  DCHECK(url_request_context_getter_);
}

ProfileSyncComponentsFactoryImpl::~ProfileSyncComponentsFactoryImpl() {
}

void ProfileSyncComponentsFactoryImpl::RegisterDataTypes(
    ProfileSyncService* pss) {
  syncer::ModelTypeSet disabled_types =
      GetDisabledTypesFromCommandLine(*command_line_);
  syncer::ModelTypeSet enabled_types =
      GetEnabledTypesFromCommandLine(*command_line_);
  RegisterCommonDataTypes(disabled_types, enabled_types, pss);
#if !defined(OS_ANDROID)
  RegisterDesktopDataTypes(disabled_types, enabled_types, pss);
#endif
}

void ProfileSyncComponentsFactoryImpl::RegisterCommonDataTypes(
    syncer::ModelTypeSet disabled_types,
    syncer::ModelTypeSet enabled_types,
    ProfileSyncService* pss) {
  // Autofill sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::AUTOFILL)) {
    pss->RegisterDataTypeController(
        new AutofillDataTypeController(this, profile_));
  }

  // Autofill profile sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::AUTOFILL_PROFILE)) {
    pss->RegisterDataTypeController(
        new AutofillProfileDataTypeController(this, profile_));
  }

  // Bookmark sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::BOOKMARKS)) {
    pss->RegisterDataTypeController(
        new BookmarkDataTypeController(this, profile_, pss));
  }

  // TypedUrl sync is enabled by default.  Register unless explicitly disabled,
  // or if saving history is disabled.
  if (!profile_->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled) &&
      !disabled_types.Has(syncer::TYPED_URLS)) {
    pss->RegisterDataTypeController(
        new TypedUrlDataTypeController(this, profile_, pss));
  }

  // Delete directive sync is enabled by default.  Register unless full history
  // sync is disabled.
  if (!disabled_types.Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::HISTORY_DELETE_DIRECTIVES,
            this));
  }

  // Session sync is enabled by default.  Register unless explicitly disabled.
  if (!disabled_types.Has(syncer::PROXY_TABS)) {
    pss->RegisterDataTypeController(new ProxyDataTypeController(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
        syncer::PROXY_TABS));
    pss->RegisterDataTypeController(
        new SessionDataTypeController(this,
                                      profile_,
                                      pss->GetSyncedWindowDelegatesGetter(),
                                      pss->GetLocalDeviceInfoProvider()));
  }

  // Favicon sync is enabled by default. Register unless explicitly disabled.
  if (!disabled_types.Has(syncer::FAVICON_IMAGES) &&
      !disabled_types.Has(syncer::FAVICON_TRACKING)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::FAVICON_IMAGES,
            this));
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::FAVICON_TRACKING,
            this));
  }

  // Password sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::PASSWORDS)) {
    pss->RegisterDataTypeController(
        new PasswordDataTypeController(this, profile_));
  }

  // Article sync is disabled by default.  Register only if explicitly enabled.
  if (IsEnableSyncArticlesSet()) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::ARTICLES,
            this));
  }

#if defined(ENABLE_MANAGED_USERS)
  pss->RegisterDataTypeController(
      new SupervisedUserSyncDataTypeController(
          syncer::SUPERVISED_USER_SETTINGS,
          this,
          profile_));
  pss->RegisterDataTypeController(
      new SupervisedUserSyncDataTypeController(
          syncer::SUPERVISED_USERS,
          this,
          profile_));
  pss->RegisterDataTypeController(
      new SupervisedUserSyncDataTypeController(
          syncer::SUPERVISED_USER_SHARED_SETTINGS,
          this,
          profile_));
#endif
}

void ProfileSyncComponentsFactoryImpl::RegisterDesktopDataTypes(
    syncer::ModelTypeSet disabled_types,
    syncer::ModelTypeSet enabled_types,
    ProfileSyncService* pss) {
  // App sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::APPS)) {
    pss->RegisterDataTypeController(
        new ExtensionDataTypeController(syncer::APPS, this, profile_));
  }

  // Extension sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::EXTENSIONS)) {
    pss->RegisterDataTypeController(
        new ExtensionDataTypeController(syncer::EXTENSIONS, this, profile_));
  }

  // Preference sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::PREFERENCES)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::PREFERENCES,
            this));
  }

  if (!disabled_types.Has(syncer::PRIORITY_PREFERENCES)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::PRIORITY_PREFERENCES,
            this));
  }

#if defined(ENABLE_THEMES)
  // Theme sync is enabled by default.  Register unless explicitly disabled.
  if (!disabled_types.Has(syncer::THEMES)) {
    pss->RegisterDataTypeController(
        new ThemeDataTypeController(this, profile_));
  }
#endif

  // Search Engine sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::SEARCH_ENGINES)) {
    pss->RegisterDataTypeController(
        new SearchEngineDataTypeController(this, profile_));
  }

  // Extension setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::EXTENSION_SETTINGS)) {
    pss->RegisterDataTypeController(new ExtensionSettingDataTypeController(
        syncer::EXTENSION_SETTINGS, this, profile_));
  }

  // App setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::APP_SETTINGS)) {
    pss->RegisterDataTypeController(new ExtensionSettingDataTypeController(
        syncer::APP_SETTINGS, this, profile_));
  }

#if defined(ENABLE_APP_LIST)
  if (app_list::switches::IsAppListSyncEnabled()) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::APP_LIST,
            this));
  }
#endif

  // Synced Notifications are disabled by default.
#if defined(ENABLE_EXTENSIONS) && defined(ENABLE_NOTIFICATIONS)
  if (enabled_types.Has(syncer::SYNCED_NOTIFICATIONS)) {
    pss->RegisterDataTypeController(
        new ExtensionBackedDataTypeController(
              syncer::SYNCED_NOTIFICATIONS,
              "",  // TODO(dewittj): pass the extension hash here.
              this,
              profile_));

    pss->RegisterDataTypeController(
        new ExtensionBackedDataTypeController(
              syncer::SYNCED_NOTIFICATION_APP_INFO,
              "",  // TODO(dewittj): pass the extension hash here.
              this,
              profile_));
  }
#endif

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS)
  // Dictionary sync is enabled by default.
  if (!disabled_types.Has(syncer::DICTIONARY)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::DICTIONARY,
            this));
  }
#endif
}

DataTypeManager* ProfileSyncComponentsFactoryImpl::CreateDataTypeManager(
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const DataTypeController::TypeMap* controllers,
    const sync_driver::DataTypeEncryptionHandler* encryption_handler,
    SyncBackendHost* backend,
    DataTypeManagerObserver* observer,
    sync_driver::FailedDataTypesHandler* failed_data_types_handler) {
  return new DataTypeManagerImpl(base::Bind(ChromeReportUnrecoverableError),
                                 debug_info_listener,
                                 controllers,
                                 encryption_handler,
                                 backend,
                                 observer,
                                 failed_data_types_handler);
}

browser_sync::SyncBackendHost*
ProfileSyncComponentsFactoryImpl::CreateSyncBackendHost(
    const std::string& name,
    Profile* profile,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
    const base::FilePath& sync_folder) {
  return new browser_sync::SyncBackendHostImpl(name, profile, invalidator,
                                               sync_prefs, sync_folder);
}

scoped_ptr<browser_sync::LocalDeviceInfoProvider>
ProfileSyncComponentsFactoryImpl::CreateLocalDeviceInfoProvider() {
  return scoped_ptr<browser_sync::LocalDeviceInfoProvider>(
      new browser_sync::LocalDeviceInfoProviderImpl());
}

base::WeakPtr<syncer::SyncableService> ProfileSyncComponentsFactoryImpl::
    GetSyncableServiceForType(syncer::ModelType type) {
  if (!profile_) {  // For tests.
     return base::WeakPtr<syncer::SyncableService>();
  }
  switch (type) {
    case syncer::PREFERENCES:
      return PrefServiceSyncable::FromProfile(
          profile_)->GetSyncableService(syncer::PREFERENCES)->AsWeakPtr();
    case syncer::PRIORITY_PREFERENCES:
      return PrefServiceSyncable::FromProfile(profile_)->GetSyncableService(
          syncer::PRIORITY_PREFERENCES)->AsWeakPtr();
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_PROFILE: {
      if (!web_data_service_.get())
        return base::WeakPtr<syncer::SyncableService>();
      if (type == syncer::AUTOFILL) {
        return AutocompleteSyncableService::FromWebDataService(
            web_data_service_.get())->AsWeakPtr();
      } else {
        return autofill::AutofillProfileSyncableService::FromWebDataService(
            web_data_service_.get())->AsWeakPtr();
      }
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
      HistoryService* history =
          HistoryServiceFactory::GetForProfile(
              profile_, Profile::EXPLICIT_ACCESS);
      return history ? history->AsWeakPtr() : base::WeakPtr<HistoryService>();
    }
#if defined(ENABLE_EXTENSIONS)
    case syncer::SYNCED_NOTIFICATIONS:
    case syncer::SYNCED_NOTIFICATION_APP_INFO: {
      return notifier::ChromeNotifierServiceFactory::GetForProfile(
                 profile_, Profile::IMPLICIT_ACCESS)
          ->GetSyncedNotificationsShim()
          ->AsWeakPtr();
    }
#endif
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
#if defined(ENABLE_MANAGED_USERS)
    case syncer::SUPERVISED_USER_SETTINGS:
      return SupervisedUserSettingsServiceFactory::GetForProfile(profile_)->
          AsWeakPtr();
    case syncer::SUPERVISED_USERS:
      return SupervisedUserSyncServiceFactory::GetForProfile(profile_)->
          AsWeakPtr();
    case syncer::SUPERVISED_USER_SHARED_SETTINGS:
      return SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
          profile_)->AsWeakPtr();
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
#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
      password_manager::PasswordStore* password_store =
          PasswordStoreFactory::GetForProfile(profile_,
                                              Profile::EXPLICIT_ACCESS);
      return password_store ? password_store->GetPasswordSyncableService()
                            : base::WeakPtr<syncer::SyncableService>();
#else
      return base::WeakPtr<syncer::SyncableService>();
#endif
    }
    default:
      // The following datatypes still need to be transitioned to the
      // syncer::SyncableService API:
      // Bookmarks
      // Typed URLs
      NOTREACHED();
      return base::WeakPtr<syncer::SyncableService>();
  }
}

class TokenServiceProvider
    : public OAuth2TokenServiceRequest::TokenServiceProvider {
 public:
  TokenServiceProvider(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      OAuth2TokenService* token_service);

  // OAuth2TokenServiceRequest::TokenServiceProvider implementation.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetTokenServiceTaskRunner() OVERRIDE;
  virtual OAuth2TokenService* GetTokenService() OVERRIDE;

 private:
  virtual ~TokenServiceProvider();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  OAuth2TokenService* token_service_;
};

TokenServiceProvider::TokenServiceProvider(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    OAuth2TokenService* token_service)
    : task_runner_(task_runner), token_service_(token_service) {
}

TokenServiceProvider::~TokenServiceProvider() {
}

scoped_refptr<base::SingleThreadTaskRunner>
TokenServiceProvider::GetTokenServiceTaskRunner() {
  return task_runner_;
}

OAuth2TokenService* TokenServiceProvider::GetTokenService() {
  return token_service_;
}

scoped_ptr<syncer::AttachmentService>
ProfileSyncComponentsFactoryImpl::CreateAttachmentService(
    const syncer::UserShare& user_share,
    syncer::AttachmentService::Delegate* delegate) {

  scoped_ptr<syncer::AttachmentStore> attachment_store(
      new syncer::FakeAttachmentStore(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));

  scoped_ptr<syncer::AttachmentUploader> attachment_uploader;
  scoped_ptr<syncer::AttachmentDownloader> attachment_downloader;
  // Only construct an AttachmentUploader and AttachmentDownload if we have sync
  // credentials. We may not have sync credentials because there may not be a
  // signed in sync user (e.g. sync is running in "backup" mode).
  if (!user_share.sync_credentials.email.empty() &&
      !user_share.sync_credentials.scope_set.empty()) {
    scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>
        token_service_provider(new TokenServiceProvider(
            content::BrowserThread::GetMessageLoopProxyForThread(
                content::BrowserThread::UI),
            token_service_));
    // TODO(maniscalco): Use shared (one per profile) thread-safe instances of
    // AttachmentUploader and AttachmentDownloader instead of creating a new one
    // per AttachmentService (bug 369536).
    attachment_uploader.reset(new syncer::AttachmentUploaderImpl(
        sync_service_url_,
        url_request_context_getter_,
        user_share.sync_credentials.email,
        user_share.sync_credentials.scope_set,
        token_service_provider));

    token_service_provider = new TokenServiceProvider(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::UI),
        token_service_);
    attachment_downloader = syncer::AttachmentDownloader::Create(
        sync_service_url_,
        url_request_context_getter_,
        user_share.sync_credentials.email,
        user_share.sync_credentials.scope_set,
        token_service_provider);
  }

  scoped_ptr<syncer::AttachmentService> attachment_service(
      new syncer::AttachmentServiceImpl(attachment_store.Pass(),
                                        attachment_uploader.Pass(),
                                        attachment_downloader.Pass(),
                                        delegate));

  return attachment_service.Pass();
}

ProfileSyncComponentsFactory::SyncComponents
    ProfileSyncComponentsFactoryImpl::CreateBookmarkSyncComponents(
        ProfileSyncService* profile_sync_service,
        sync_driver::DataTypeErrorHandler* error_handler) {
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfile(profile_sync_service->profile());
  syncer::UserShare* user_share = profile_sync_service->GetUserShare();
  // TODO(akalin): We may want to propagate this switch up eventually.
#if defined(OS_ANDROID)
  const bool kExpectMobileBookmarksFolder = true;
#else
  const bool kExpectMobileBookmarksFolder = false;
#endif
  BookmarkModelAssociator* model_associator =
      new BookmarkModelAssociator(bookmark_model,
                                  profile_sync_service->profile(),
                                  user_share,
                                  error_handler,
                                  kExpectMobileBookmarksFolder);
  BookmarkChangeProcessor* change_processor =
      new BookmarkChangeProcessor(profile_sync_service->profile(),
                                  model_associator,
                                  error_handler);
  return SyncComponents(model_associator, change_processor);
}

ProfileSyncComponentsFactory::SyncComponents
    ProfileSyncComponentsFactoryImpl::CreateTypedUrlSyncComponents(
        ProfileSyncService* profile_sync_service,
        history::HistoryBackend* history_backend,
        sync_driver::DataTypeErrorHandler* error_handler) {
  TypedUrlModelAssociator* model_associator =
      new TypedUrlModelAssociator(profile_sync_service,
                                  history_backend,
                                  error_handler);
  TypedUrlChangeProcessor* change_processor =
      new TypedUrlChangeProcessor(profile_,
                                  model_associator,
                                  history_backend,
                                  error_handler);
  return SyncComponents(model_associator, change_processor);
}
