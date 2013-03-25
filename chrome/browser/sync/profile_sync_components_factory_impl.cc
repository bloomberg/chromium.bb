// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/api/webdata/autofill_web_data_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#if !defined(OS_ANDROID)
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#endif
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/data_type_manager_observer.h"
#include "chrome/browser/sync/glue/extension_data_type_controller.h"
#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/glue/password_change_processor.h"
#include "chrome/browser/sync/glue/password_data_type_controller.h"
#include "chrome/browser/sync/glue/password_model_associator.h"
#include "chrome/browser/sync/glue/proxy_data_type_controller.h"
#include "chrome/browser/sync/glue/search_engine_data_type_controller.h"
#include "chrome/browser/sync/glue/session_change_processor.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/shared_change_processor.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/theme_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_change_processor.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/glue/ui_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/webdata/autocomplete_syncable_service.h"
#include "chrome/browser/webdata/autofill_profile_syncable_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/syncable_service.h"

using browser_sync::AutofillDataTypeController;
using browser_sync::AutofillProfileDataTypeController;
using browser_sync::BookmarkChangeProcessor;
using browser_sync::BookmarkDataTypeController;
using browser_sync::BookmarkModelAssociator;
using browser_sync::DataTypeController;
using browser_sync::DataTypeErrorHandler;
using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerImpl;
using browser_sync::DataTypeManagerObserver;
using browser_sync::ExtensionDataTypeController;
using browser_sync::ExtensionSettingDataTypeController;
using browser_sync::GenericChangeProcessor;
using browser_sync::PasswordChangeProcessor;
using browser_sync::PasswordDataTypeController;
using browser_sync::PasswordModelAssociator;
using browser_sync::ProxyDataTypeController;
using browser_sync::SearchEngineDataTypeController;
using browser_sync::SessionChangeProcessor;
using browser_sync::SessionDataTypeController;
using browser_sync::SessionModelAssociator;
using browser_sync::SharedChangeProcessor;
using browser_sync::SyncBackendHost;
using browser_sync::ThemeDataTypeController;
using browser_sync::TypedUrlChangeProcessor;
using browser_sync::TypedUrlDataTypeController;
using browser_sync::TypedUrlModelAssociator;
using browser_sync::UIDataTypeController;
using content::BrowserThread;

ProfileSyncComponentsFactoryImpl::ProfileSyncComponentsFactoryImpl(
    Profile* profile, CommandLine* command_line)
    : profile_(profile),
      command_line_(command_line),
      extension_system_(
          extensions::ExtensionSystemFactory::GetForProfile(profile)),
      web_data_service_(AutofillWebDataService::FromBrowserContext(profile_)) {
}

ProfileSyncComponentsFactoryImpl::~ProfileSyncComponentsFactoryImpl() {
}

void ProfileSyncComponentsFactoryImpl::RegisterDataTypes(
    ProfileSyncService* pss) {
  RegisterCommonDataTypes(pss);
#if !defined(OS_ANDROID)
  RegisterDesktopDataTypes(pss);
#endif
}

void ProfileSyncComponentsFactoryImpl::RegisterCommonDataTypes(
    ProfileSyncService* pss) {
  // Autofill sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncAutofill)) {
    pss->RegisterDataTypeController(
        new AutofillDataTypeController(this, profile_, pss));
  }

  // Autofill profile sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncAutofillProfile)) {
    pss->RegisterDataTypeController(
        new AutofillProfileDataTypeController(this, profile_, pss));
  }

  // Bookmark sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncBookmarks)) {
    pss->RegisterDataTypeController(
        new BookmarkDataTypeController(this, profile_, pss));
  }

  // TypedUrl sync is enabled by default.  Register unless explicitly disabled,
  // or if saving history is disabled.
  if (!profile_->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled) &&
      !command_line_->HasSwitch(switches::kDisableSyncTypedUrls)) {
    pss->RegisterDataTypeController(
        new TypedUrlDataTypeController(this, profile_, pss));
  }

  // Unless it is explicitly disabled, history delete directive sync is
  // enabled whenever full history sync is enabled.
  if (command_line_->HasSwitch(switches::kHistoryEnableFullHistorySync) &&
      !command_line_->HasSwitch(
          switches::kDisableSyncHistoryDeleteDirectives)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            syncer::HISTORY_DELETE_DIRECTIVES, this, profile_, pss));
  }

  // Session sync is enabled by default.  Register unless explicitly disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncTabs)) {
    pss->RegisterDataTypeController(
        new ProxyDataTypeController(syncer::PROXY_TABS));
    pss->RegisterDataTypeController(
        new SessionDataTypeController(this, profile_, pss));
  }

  if (command_line_->HasSwitch(switches::kEnableSyncFavicons)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(syncer::FAVICON_IMAGES,
                                 this,
                                 profile_,
                                 pss));
    pss->RegisterDataTypeController(
        new UIDataTypeController(syncer::FAVICON_TRACKING,
                                 this,
                                 profile_,
                                 pss));
  }

  // Password sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncPasswords)) {
    pss->RegisterDataTypeController(
        new PasswordDataTypeController(this, profile_, pss));
  }
}

void ProfileSyncComponentsFactoryImpl::RegisterDesktopDataTypes(
    ProfileSyncService* pss) {
  // App sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncApps)) {
    pss->RegisterDataTypeController(
        new ExtensionDataTypeController(syncer::APPS, this, profile_, pss));
  }

  // Extension sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncExtensions)) {
    pss->RegisterDataTypeController(
        new ExtensionDataTypeController(syncer::EXTENSIONS,
                                        this, profile_, pss));
  }

  // Preference sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncPreferences)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(syncer::PREFERENCES, this, profile_, pss));
  }

#if defined(ENABLE_THEMES)
  // Theme sync is enabled by default.  Register unless explicitly disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncThemes)) {
    pss->RegisterDataTypeController(
        new ThemeDataTypeController(this, profile_, pss));
  }
#endif

  // Search Engine sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncSearchEngines)) {
    pss->RegisterDataTypeController(
        new SearchEngineDataTypeController(this, profile_, pss));
  }

  // Extension setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncExtensionSettings)) {
    pss->RegisterDataTypeController(
        new ExtensionSettingDataTypeController(
            syncer::EXTENSION_SETTINGS, this, profile_, pss));
  }

  // App setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!command_line_->HasSwitch(switches::kDisableSyncAppSettings)) {
    pss->RegisterDataTypeController(
        new ExtensionSettingDataTypeController(
            syncer::APP_SETTINGS, this, profile_, pss));
  }

  // Synced Notifications sync is disabled by default.
  // TODO(petewil): Switch to enabled by default once datatype support is done.
  if (command_line_->HasSwitch(switches::kEnableSyncSyncedNotifications)) {
#if !defined(OS_ANDROID)
    pss->RegisterDataTypeController(
        new UIDataTypeController(
            syncer::SYNCED_NOTIFICATIONS, this, profile_, pss));
#endif
  }

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS)
  // Dictionary sync is enabled by default.
  if (!command_line_->HasSwitch(switches::kDisableSyncDictionary)) {
    pss->RegisterDataTypeController(
        new UIDataTypeController(syncer::DICTIONARY, this, profile_, pss));
  }
#endif

}

DataTypeManager* ProfileSyncComponentsFactoryImpl::CreateDataTypeManager(
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    SyncBackendHost* backend,
    const DataTypeController::TypeMap* controllers,
    DataTypeManagerObserver* observer,
    const FailedDatatypesHandler* failed_datatypes_handler) {
  return new DataTypeManagerImpl(debug_info_listener,
                                 backend,
                                 controllers,
                                 observer,
                                 failed_datatypes_handler);
}

browser_sync::GenericChangeProcessor*
    ProfileSyncComponentsFactoryImpl::CreateGenericChangeProcessor(
        ProfileSyncService* profile_sync_service,
        browser_sync::DataTypeErrorHandler* error_handler,
        const base::WeakPtr<syncer::SyncableService>& local_service,
        const base::WeakPtr<syncer::SyncMergeResult>& merge_result) {
  syncer::UserShare* user_share = profile_sync_service->GetUserShare();
  return new GenericChangeProcessor(error_handler,
                                    local_service,
                                    merge_result,
                                    user_share);
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
          profile_)->GetSyncableService()->AsWeakPtr();
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_PROFILE: {
      if (!web_data_service_.get())
        return base::WeakPtr<syncer::SyncableService>();
      if (type == syncer::AUTOFILL) {
        return AutocompleteSyncableService::FromWebDataService(
            web_data_service_)->AsWeakPtr();
      } else {
        return AutofillProfileSyncableService::FromWebDataService(
            web_data_service_)->AsWeakPtr();
      }
    }
    case syncer::APPS:
    case syncer::EXTENSIONS:
      return extension_system_->extension_service()->AsWeakPtr();
    case syncer::SEARCH_ENGINES:
      return TemplateURLServiceFactory::GetForProfile(profile_)->AsWeakPtr();
    case syncer::APP_SETTINGS:
    case syncer::EXTENSION_SETTINGS:
      return extension_system_->extension_service()->settings_frontend()->
          GetBackendForSync(type)->AsWeakPtr();
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
#endif
    case syncer::DICTIONARY:
      return SpellcheckServiceFactory::GetForProfile(profile_)->
          GetCustomDictionary()->AsWeakPtr();
    case syncer::FAVICON_IMAGES:
    case syncer::FAVICON_TRACKING:
      return ProfileSyncServiceFactory::GetForProfile(profile_)->
          GetSessionModelAssociator()->GetFaviconCache()->AsWeakPtr();
    default:
      // The following datatypes still need to be transitioned to the
      // syncer::SyncableService API:
      // Bookmarks
      // Passwords
      // Sessions
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
                                  user_share,
                                  error_handler,
                                  kExpectMobileBookmarksFolder);
  BookmarkChangeProcessor* change_processor =
      new BookmarkChangeProcessor(model_associator,
                                  error_handler);
  return SyncComponents(model_associator, change_processor);
}

ProfileSyncComponentsFactory::SyncComponents
    ProfileSyncComponentsFactoryImpl::CreatePasswordSyncComponents(
        ProfileSyncService* profile_sync_service,
        PasswordStore* password_store,
        DataTypeErrorHandler* error_handler) {
  PasswordModelAssociator* model_associator =
      new PasswordModelAssociator(profile_sync_service,
                                  password_store,
                                  error_handler);
  PasswordChangeProcessor* change_processor =
      new PasswordChangeProcessor(model_associator,
                                  password_store,
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
