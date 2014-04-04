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
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/pref_service_flags_storage.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/extension_data_type_controller.h"
#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/glue/password_data_type_controller.h"
#include "chrome/browser/sync/glue/search_engine_data_type_controller.h"
#include "chrome/browser/sync/glue/session_change_processor.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/shared_change_processor.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/sync_backend_host_impl.h"
#include "chrome/browser/sync/glue/theme_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_change_processor.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/glue/ui_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sessions2/session_data_type_controller2.h"
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
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/data_type_manager_observer.h"
#include "components/sync_driver/proxy_data_type_controller.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "sync/api/attachments/attachment_service.h"
#include "sync/api/attachments/fake_attachment_service.h"
#include "sync/api/attachments/fake_attachment_store.h"
#include "sync/api/syncable_service.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#endif

#if defined(ENABLE_APP_LIST)
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "ui/app_list/app_list_switches.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_settings_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service_factory.h"
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
using browser_sync::DataTypeController;
using browser_sync::DataTypeErrorHandler;
using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerImpl;
using browser_sync::DataTypeManagerObserver;
using browser_sync::ExtensionDataTypeController;
using browser_sync::ExtensionSettingDataTypeController;
using browser_sync::GenericChangeProcessor;
using browser_sync::PasswordDataTypeController;
using browser_sync::ProxyDataTypeController;
using browser_sync::SearchEngineDataTypeController;
using browser_sync::SessionChangeProcessor;
using browser_sync::SessionDataTypeController;
using browser_sync::SessionDataTypeController2;
using browser_sync::SessionModelAssociator;
using browser_sync::SharedChangeProcessor;
using browser_sync::SyncBackendHost;
using browser_sync::ThemeDataTypeController;
using browser_sync::TypedUrlChangeProcessor;
using browser_sync::TypedUrlDataTypeController;
using browser_sync::TypedUrlModelAssociator;
using browser_sync::UIDataTypeController;
using content::BrowserThread;

namespace {

syncer::ModelTypeSet GetDisabledTypesFromCommandLine(
    CommandLine* command_line) {
  syncer::ModelTypeSet disabled_types;
  std::string disabled_types_str =
      command_line->GetSwitchValueASCII(switches::kDisableSyncTypes);
  disabled_types = syncer::ModelTypeSetFromString(disabled_types_str);
  return disabled_types;
}

}  // namespace

ProfileSyncComponentsFactoryImpl::ProfileSyncComponentsFactoryImpl(
    Profile* profile, CommandLine* command_line)
    : profile_(profile),
      command_line_(command_line),
      extension_system_(extensions::ExtensionSystem::Get(profile)),
      web_data_service_(
          WebDataServiceFactory::GetAutofillWebDataForProfile(
              profile_, Profile::EXPLICIT_ACCESS)) {
}

ProfileSyncComponentsFactoryImpl::~ProfileSyncComponentsFactoryImpl() {
}

void ProfileSyncComponentsFactoryImpl::RegisterDataTypes(
    ProfileSyncService* pss) {
  syncer::ModelTypeSet disabled_types =
      GetDisabledTypesFromCommandLine(command_line_);
  // TODO(zea): pass an enabled_types set for types that are off by default.
  RegisterCommonDataTypes(disabled_types, pss);
#if !defined(OS_ANDROID)
  RegisterDesktopDataTypes(disabled_types, pss);
#endif
}

void ProfileSyncComponentsFactoryImpl::RegisterCommonDataTypes(
    syncer::ModelTypeSet disabled_types,
    ProfileSyncService* pss) {
  // Autofill sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::AUTOFILL)) {
    pss->RegisterDataTypeController(
        new AutofillDataTypeController(this, profile_, pss));
  }

  // Autofill profile sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::AUTOFILL_PROFILE)) {
    pss->RegisterDataTypeController(
        new AutofillProfileDataTypeController(this, profile_, pss));
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
            this,
            profile_,
            pss));
  }

  // Session sync is enabled by default.  Register unless explicitly disabled.
  if (!disabled_types.Has(syncer::PROXY_TABS)) {
      pss->RegisterDataTypeController(new ProxyDataTypeController(
         BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
         syncer::PROXY_TABS));
    pss->RegisterDataTypeController(
        new SessionDataTypeController2(this, profile_, pss));
  }

  // Favicon sync is enabled by default. Register unless explicitly disabled.
  if (!disabled_types.Has(syncer::FAVICON_IMAGES) &&
      !disabled_types.Has(syncer::FAVICON_TRACKING)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::FAVICON_IMAGES,
            this,
            profile_,
            pss));
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::FAVICON_TRACKING,
            this,
            profile_,
            pss));
  }

  // Password sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::PASSWORDS)) {
    pss->RegisterDataTypeController(
        new PasswordDataTypeController(this, profile_, pss));
  }

  // Article sync is disabled by default.  Register only if explicitly enabled.
  if (IsEnableSyncArticlesSet()) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::ARTICLES,
            this,
            profile_,
            pss));
  }

#if defined(ENABLE_MANAGED_USERS)
  if (profile_->IsManaged()) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::MANAGED_USER_SETTINGS,
            this,
            profile_,
            pss));
  } else {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::MANAGED_USERS,
            this,
            profile_,
            pss));
  }
  pss->RegisterDataTypeController(
      new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::MANAGED_USER_SHARED_SETTINGS,
            this,
            profile_,
            pss));
#endif
}

void ProfileSyncComponentsFactoryImpl::RegisterDesktopDataTypes(
    syncer::ModelTypeSet disabled_types,
    ProfileSyncService* pss) {
  // App sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::APPS)) {
    pss->RegisterDataTypeController(
        new ExtensionDataTypeController(syncer::APPS, this, profile_, pss));
  }

  // Extension sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::EXTENSIONS)) {
    pss->RegisterDataTypeController(
        new ExtensionDataTypeController(syncer::EXTENSIONS,
                                        this, profile_, pss));
  }

  // Preference sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::PREFERENCES)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::PREFERENCES,
            this,
            profile_,
            pss));

  }

  if (!disabled_types.Has(syncer::PRIORITY_PREFERENCES)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::PRIORITY_PREFERENCES,
            this,
            profile_,
            pss));
  }

#if defined(ENABLE_THEMES)
  // Theme sync is enabled by default.  Register unless explicitly disabled.
  if (!disabled_types.Has(syncer::THEMES)) {
    pss->RegisterDataTypeController(
        new ThemeDataTypeController(this, profile_, pss));
  }
#endif

  // Search Engine sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::SEARCH_ENGINES)) {
    pss->RegisterDataTypeController(
        new SearchEngineDataTypeController(this, profile_, pss));
  }

  // Extension setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::EXTENSION_SETTINGS)) {
    pss->RegisterDataTypeController(
        new ExtensionSettingDataTypeController(
            syncer::EXTENSION_SETTINGS, this, profile_, pss));
  }

  // App setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::APP_SETTINGS)) {
    pss->RegisterDataTypeController(
        new ExtensionSettingDataTypeController(
            syncer::APP_SETTINGS, this, profile_, pss));
  }

#if defined(ENABLE_APP_LIST)
  if (app_list::switches::IsAppListSyncEnabled()) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::APP_LIST,
            this,
            profile_,
            pss));
  }
#endif

  // Synced Notifications are enabled by default.
  if (!disabled_types.Has(syncer::SYNCED_NOTIFICATIONS)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
              BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
              base::Bind(&ChromeReportUnrecoverableError),
              syncer::SYNCED_NOTIFICATIONS,
              this,
              profile_,
              pss));

    // Synced Notification App Infos are disabled by default.
    if (command_line_->HasSwitch(switches::kEnableSyncSyncedNotifications)) {
      pss->RegisterDataTypeController(new UIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          syncer::SYNCED_NOTIFICATION_APP_INFO,
          this,
          profile_,
          pss));
    }
  }

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS)
  // Dictionary sync is enabled by default.
  if (!disabled_types.Has(syncer::DICTIONARY)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            base::Bind(&ChromeReportUnrecoverableError),
            syncer::DICTIONARY,
            this,
            profile_,
            pss));
  }
#endif
}

DataTypeManager* ProfileSyncComponentsFactoryImpl::CreateDataTypeManager(
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const DataTypeController::TypeMap* controllers,
    const browser_sync::DataTypeEncryptionHandler* encryption_handler,
    SyncBackendHost* backend,
    DataTypeManagerObserver* observer,
    browser_sync::FailedDataTypesHandler* failed_data_types_handler) {
  return new DataTypeManagerImpl(debug_info_listener,
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
    const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs) {
  return new browser_sync::SyncBackendHostImpl(name, profile, sync_prefs);
}

browser_sync::GenericChangeProcessor*
    ProfileSyncComponentsFactoryImpl::CreateGenericChangeProcessor(
        ProfileSyncService* profile_sync_service,
        browser_sync::DataTypeErrorHandler* error_handler,
        const base::WeakPtr<syncer::SyncableService>& local_service,
        const base::WeakPtr<syncer::SyncMergeResult>& merge_result) {
  syncer::UserShare* user_share = profile_sync_service->GetUserShare();
  // TODO(maniscalco): Replace FakeAttachmentService with a real
  // AttachmentService implementation once it has been implemented (bug 356359).
  scoped_ptr<syncer::AttachmentStore> attachment_store(
      new syncer::FakeAttachmentStore(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
  scoped_ptr<syncer::AttachmentService> attachment_service(
      new syncer::FakeAttachmentService(attachment_store.Pass()));
  return new GenericChangeProcessor(
      error_handler,
      local_service,
      merge_result,
      user_share,
      attachment_service.Pass());
}

browser_sync::SharedChangeProcessor* ProfileSyncComponentsFactoryImpl::
    CreateSharedChangeProcessor() {
  return new SharedChangeProcessor();
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
#if !defined(OS_ANDROID)
    case syncer::SYNCED_NOTIFICATIONS: {
      notifier::ChromeNotifierService* notifier_service =
          notifier::ChromeNotifierServiceFactory::GetForProfile(
              profile_, Profile::EXPLICIT_ACCESS);
      return notifier_service ? notifier_service->AsWeakPtr()
          : base::WeakPtr<syncer::SyncableService>();
    }

    case syncer::SYNCED_NOTIFICATION_APP_INFO: {
      notifier::SyncedNotificationAppInfoService* app_info =
          notifier::SyncedNotificationAppInfoServiceFactory::GetForProfile(
              profile_, Profile::EXPLICIT_ACCESS);
      return app_info ? app_info->AsWeakPtr()
                      : base::WeakPtr<syncer::SyncableService>();
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
    case syncer::MANAGED_USER_SETTINGS:
      return ManagedUserSettingsServiceFactory::GetForProfile(profile_)->
          AsWeakPtr();
    case syncer::MANAGED_USERS:
      return ManagedUserSyncServiceFactory::GetForProfile(profile_)->
          AsWeakPtr();
    case syncer::MANAGED_USER_SHARED_SETTINGS:
      return ManagedUserSharedSettingsServiceFactory::GetForBrowserContext(
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
      DCHECK(!command_line_->HasSwitch(switches::kDisableSyncSessionsV2));
      return ProfileSyncServiceFactory::GetForProfile(profile_)->
          GetSessionsSyncableService()->AsWeakPtr();
    }
    case syncer::PASSWORDS: {
#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
      PasswordStore* password_store = PasswordStoreFactory::GetForProfile(
          profile_, Profile::EXPLICIT_ACCESS);
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

ProfileSyncComponentsFactory::SyncComponents
    ProfileSyncComponentsFactoryImpl::CreateBookmarkSyncComponents(
        ProfileSyncService* profile_sync_service,
        DataTypeErrorHandler* error_handler) {
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
      new BookmarkChangeProcessor(model_associator,
                                  error_handler);
  return SyncComponents(model_associator, change_processor);
}

ProfileSyncComponentsFactory::SyncComponents
    ProfileSyncComponentsFactoryImpl::CreateTypedUrlSyncComponents(
        ProfileSyncService* profile_sync_service,
        history::HistoryBackend* history_backend,
        browser_sync::DataTypeErrorHandler* error_handler) {
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

ProfileSyncComponentsFactory::SyncComponents
    ProfileSyncComponentsFactoryImpl::CreateSessionSyncComponents(
       ProfileSyncService* profile_sync_service,
        DataTypeErrorHandler* error_handler) {
  SessionModelAssociator* model_associator =
      new SessionModelAssociator(profile_sync_service, error_handler);
  SessionChangeProcessor* change_processor =
      new SessionChangeProcessor(error_handler, model_associator);
  return SyncComponents(model_associator, change_processor);
}
