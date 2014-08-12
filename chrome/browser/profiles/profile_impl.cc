// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/version.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/shortcuts_backend.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/dom_distiller/lazy_dom_distiller_service.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/tracked/tracked_preference_validation_delegate.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/services/gcm/push_messaging_service_impl.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/dom_distiller/content/dom_distiller_viewer_source.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/domain_reliability/monitor.h"
#include "components/domain_reliability/service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "components/url_fixer/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/page_zoom.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/locale_change_guard.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

#if defined(SPDY_PROXY_AUTH_ORIGIN)
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_configurator.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

using base::Time;
using base::TimeDelta;
using base::UserMetricsAction;
using content::BrowserThread;
using content::DownloadManagerDelegate;
using content::HostZoomMap;

namespace {

#if defined(ENABLE_SESSION_SERVICE)
// Delay, in milliseconds, before we explicitly create the SessionService.
const int kCreateSessionServiceDelayMS = 500;
#endif

// Text content of README file created in each profile directory. Both %s
// placeholders must contain the product name. This is not localizable and hence
// not in resources.
const char kReadmeText[] =
    "%s settings and storage represent user-selected preferences and "
    "information and MUST not be extracted, overwritten or modified except "
    "through %s defined APIs.";

// Value written to prefs for EXIT_CRASHED and EXIT_SESSION_ENDED.
const char kPrefExitTypeCrashed[] = "Crashed";
const char kPrefExitTypeSessionEnded[] = "SessionEnded";

// Helper method needed because PostTask cannot currently take a Callback
// function with non-void return type.
void CreateDirectoryAndSignal(const base::FilePath& path,
                              base::WaitableEvent* done_creating) {
  DVLOG(1) << "Creating directory " << path.value();
  base::CreateDirectory(path);
  done_creating->Signal();
}

// Task that blocks the FILE thread until CreateDirectoryAndSignal() finishes on
// blocking I/O pool.
void BlockFileThreadOnDirectoryCreate(base::WaitableEvent* done_creating) {
  done_creating->Wait();
}

// Initiates creation of profile directory on |sequenced_task_runner| and
// ensures that FILE thread is blocked until that operation finishes.
void CreateProfileDirectory(base::SequencedTaskRunner* sequenced_task_runner,
                            const base::FilePath& path) {
  base::WaitableEvent* done_creating = new base::WaitableEvent(false, false);
  sequenced_task_runner->PostTask(FROM_HERE,
                                  base::Bind(&CreateDirectoryAndSignal,
                                             path,
                                             done_creating));
  // Block the FILE thread until directory is created on I/O pool to make sure
  // that we don't attempt any operation until that part completes.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&BlockFileThreadOnDirectoryCreate,
                 base::Owned(done_creating)));
}

base::FilePath GetCachePath(const base::FilePath& base) {
  return base.Append(chrome::kCacheDirname);
}

base::FilePath GetMediaCachePath(const base::FilePath& base) {
  return base.Append(chrome::kMediaCacheDirname);
}

void EnsureReadmeFile(const base::FilePath& base) {
  base::FilePath readme_path = base.Append(chrome::kReadmeFilename);
  if (base::PathExists(readme_path))
    return;
  std::string product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  std::string readme_text = base::StringPrintf(
      kReadmeText, product_name.c_str(), product_name.c_str());
  if (base::WriteFile(readme_path, readme_text.data(), readme_text.size()) ==
      -1) {
    LOG(ERROR) << "Could not create README file.";
  }
}

// Converts the kSessionExitedCleanly pref to the corresponding EXIT_TYPE.
Profile::ExitType SessionTypePrefValueToExitType(const std::string& value) {
  if (value == kPrefExitTypeSessionEnded)
    return Profile::EXIT_SESSION_ENDED;
  if (value == kPrefExitTypeCrashed)
    return Profile::EXIT_CRASHED;
  return Profile::EXIT_NORMAL;
}

// Converts an ExitType into a string that is written to prefs.
std::string ExitTypeToSessionTypePrefValue(Profile::ExitType type) {
  switch (type) {
    case Profile::EXIT_NORMAL:
        return ProfileImpl::kPrefExitTypeNormal;
    case Profile::EXIT_SESSION_ENDED:
      return kPrefExitTypeSessionEnded;
    case Profile::EXIT_CRASHED:
      return kPrefExitTypeCrashed;
  }
  NOTREACHED();
  return std::string();
}

// Setup URLDataSource for the chrome-distiller:// scheme for the given
// |profile|.
void RegisterDomDistillerViewerSource(Profile* profile) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableDomDistiller)) {
    dom_distiller::DomDistillerServiceFactory* dom_distiller_service_factory =
        dom_distiller::DomDistillerServiceFactory::GetInstance();
    // The LazyDomDistillerService deletes itself when the profile is destroyed.
    dom_distiller::LazyDomDistillerService* lazy_service =
        new dom_distiller::LazyDomDistillerService(
            profile, dom_distiller_service_factory);
    content::URLDataSource::Add(
        profile,
        new dom_distiller::DomDistillerViewerSource(
            lazy_service, dom_distiller::kDomDistillerScheme));
  }
}

PrefStore* CreateExtensionPrefStore(Profile* profile,
                                    bool incognito_pref_store) {
#if defined(ENABLE_EXTENSIONS)
  return new ExtensionPrefStore(
      ExtensionPrefValueMapFactory::GetForBrowserContext(profile),
      incognito_pref_store);
#else
  return NULL;
#endif
}

#if !defined(OS_ANDROID)
// Deletes the file that was used by the AutomaticProfileResetter service, which
// has since been removed, to store that the prompt had already been shown.
// TODO(engedy): Remove this and caller in M42 or later. See crbug.com/398813.
void DeleteResetPromptMementoFile(const base::FilePath& profile_dir) {
  base::FilePath memento_path =
      profile_dir.Append(FILE_PATH_LITERAL("Reset Prompt Memento"));
  base::DeleteFile(memento_path, false);
}
#endif

}  // namespace

// static
Profile* Profile::CreateProfile(const base::FilePath& path,
                                Delegate* delegate,
                                CreateMode create_mode) {
  TRACE_EVENT_BEGIN1("browser",
                     "Profile::CreateProfile",
                     "profile_path",
                     path.value().c_str());

  // Get sequenced task runner for making sure that file operations of
  // this profile (defined by |path|) are executed in expected order
  // (what was previously assured by the FILE thread).
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(path,
                                          BrowserThread::GetBlockingPool());
  if (create_mode == CREATE_MODE_ASYNCHRONOUS) {
    DCHECK(delegate);
    CreateProfileDirectory(sequenced_task_runner.get(), path);
  } else if (create_mode == CREATE_MODE_SYNCHRONOUS) {
    if (!base::PathExists(path)) {
      // TODO(tc): http://b/1094718 Bad things happen if we can't write to the
      // profile directory.  We should eventually be able to run in this
      // situation.
      if (!base::CreateDirectory(path))
        return NULL;
    }
  } else {
    NOTREACHED();
  }

  return new ProfileImpl(
      path, delegate, create_mode, sequenced_task_runner.get());
}

// static
int ProfileImpl::create_readme_delay_ms = 60000;

// static
const char* const ProfileImpl::kPrefExitTypeNormal = "Normal";

// static
void ProfileImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kSavingBrowserHistoryDisabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAllowDeletingBrowserHistory,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSigninAllowed,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kForceSafeSearch,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kProfileAvatarIndex,
      -1,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSupervisedUserId,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kProfileName,
                               std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kHomePage,
                               std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(ENABLE_PRINTING)
  registry->RegisterBooleanPref(
      prefs::kPrintingEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(
      prefs::kPrintPreviewDisabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kForceEphemeralProfiles,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  // Initialize the cache prefs.
  registry->RegisterFilePathPref(
      prefs::kDiskCacheDir,
      base::FilePath(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kDiskCacheSize,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kMediaCacheSize,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  // Deprecated. Kept around for migration.
  registry->RegisterBooleanPref(
      prefs::kClearSiteDataOnExit,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

ProfileImpl::ProfileImpl(
    const base::FilePath& path,
    Delegate* delegate,
    CreateMode create_mode,
    base::SequencedTaskRunner* sequenced_task_runner)
    : path_(path),
      pref_registry_(new user_prefs::PrefRegistrySyncable),
      io_data_(this),
      host_content_settings_map_(NULL),
      last_session_exit_type_(EXIT_NORMAL),
      start_time_(Time::Now()),
      delegate_(delegate),
      predictor_(NULL) {
  TRACE_EVENT0("browser", "ProfileImpl::ctor")
  DCHECK(!path.empty()) << "Using an empty path will attempt to write " <<
                            "profile files to the root directory!";

#if defined(ENABLE_SESSION_SERVICE)
  create_session_service_timer_.Start(FROM_HERE,
      TimeDelta::FromMilliseconds(kCreateSessionServiceDelayMS), this,
      &ProfileImpl::EnsureSessionServiceCreated);
#endif

  // Determine if prefetch is enabled for this profile.
  // If not profile_manager is present, it means we are in a unittest.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  predictor_ = chrome_browser_net::Predictor::CreatePredictor(
      !command_line->HasSwitch(switches::kDisablePreconnect),
      !command_line->HasSwitch(switches::kDnsPrefetchDisable),
      g_browser_process->profile_manager() == NULL);

  // If we are creating the profile synchronously, then we should load the
  // policy data immediately.
  bool force_immediate_policy_load = (create_mode == CREATE_MODE_SYNCHRONOUS);
#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  schema_registry_service_ =
      policy::SchemaRegistryServiceFactory::CreateForContext(
          this, connector->GetChromeSchema(), connector->GetSchemaRegistry());
#if defined(OS_CHROMEOS)
  cloud_policy_manager_ =
      policy::UserCloudPolicyManagerFactoryChromeOS::CreateForProfile(
          this, force_immediate_policy_load, sequenced_task_runner);
#else
  cloud_policy_manager_ =
      policy::UserCloudPolicyManagerFactory::CreateForOriginalBrowserContext(
          this,
          force_immediate_policy_load,
          sequenced_task_runner,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
#endif
#endif
  profile_policy_connector_ =
      policy::ProfilePolicyConnectorFactory::CreateForProfile(
          this, force_immediate_policy_load);

  DCHECK(create_mode == CREATE_MODE_ASYNCHRONOUS ||
         create_mode == CREATE_MODE_SYNCHRONOUS);
  bool async_prefs = create_mode == CREATE_MODE_ASYNCHRONOUS;

#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(this))
    chrome::RegisterLoginProfilePrefs(pref_registry_.get());
  else
#endif
  chrome::RegisterUserProfilePrefs(pref_registry_.get());

  BrowserContextDependencyManager::GetInstance()->
      RegisterProfilePrefsForServices(this, pref_registry_.get());

  SupervisedUserSettingsService* supervised_user_settings = NULL;
#if defined(ENABLE_MANAGED_USERS)
  supervised_user_settings =
      SupervisedUserSettingsServiceFactory::GetForProfile(this);
  supervised_user_settings->Init(
      path_, sequenced_task_runner, create_mode == CREATE_MODE_SYNCHRONOUS);
#endif

  scoped_refptr<SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());
  if (safe_browsing_service) {
    pref_validation_delegate_ =
        safe_browsing_service->CreatePreferenceValidationDelegate(this).Pass();
  }

  {
    // On startup, preference loading is always synchronous so a scoped timer
    // will work here.
    startup_metric_utils::ScopedSlowStartupUMA
        scoped_timer("Startup.SlowStartupPreferenceLoading");
    prefs_ = chrome_prefs::CreateProfilePrefs(
        path_,
        sequenced_task_runner,
        pref_validation_delegate_.get(),
        profile_policy_connector_->policy_service(),
        supervised_user_settings,
        CreateExtensionPrefStore(this, false),
        pref_registry_,
        async_prefs).Pass();
    // Register on BrowserContext.
    user_prefs::UserPrefs::Set(this, prefs_.get());
  }

  startup_metric_utils::ScopedSlowStartupUMA
      scoped_timer("Startup.SlowStartupFinalProfileInit");
  if (async_prefs) {
    // Wait for the notification that prefs has been loaded
    // (successfully or not).  Note that we can use base::Unretained
    // because the PrefService is owned by this class and lives on
    // the same thread.
    prefs_->AddPrefInitObserver(base::Bind(&ProfileImpl::OnPrefsLoaded,
                                           base::Unretained(this)));
  } else {
    // Prefs were loaded synchronously so we can continue directly.
    OnPrefsLoaded(true);
  }
}

void ProfileImpl::DoFinalInit() {
  TRACE_EVENT0("browser", "ProfileImpl::DoFinalInit")
  PrefService* prefs = GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kGoogleServicesUsername,
      base::Bind(&ProfileImpl::UpdateProfileUserNameCache,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::Bind(&ProfileImpl::UpdateProfileSupervisedUserIdCache,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kDefaultZoomLevel,
      base::Bind(&ProfileImpl::OnDefaultZoomLevelChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileAvatarIndex,
      base::Bind(&ProfileImpl::UpdateProfileAvatarCache,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileName,
      base::Bind(&ProfileImpl::UpdateProfileNameCache,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kForceEphemeralProfiles,
      base::Bind(&ProfileImpl::UpdateProfileIsEphemeralCache,
                 base::Unretained(this)));

  // It would be nice to use PathService for fetching this directory, but
  // the cache directory depends on the profile directory, which isn't available
  // to PathService.
  chrome::GetUserCacheDirectory(path_, &base_cache_path_);
  // Always create the cache directory asynchronously.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(base_cache_path_,
                                          BrowserThread::GetBlockingPool());
  CreateProfileDirectory(sequenced_task_runner.get(), base_cache_path_);

  // Now that the profile is hooked up to receive pref change notifications to
  // kGoogleServicesUsername, initialize components that depend on it to reflect
  // the current value.
  UpdateProfileUserNameCache();
  UpdateProfileSupervisedUserIdCache();
  UpdateProfileIsEphemeralCache();
  GAIAInfoUpdateServiceFactory::GetForProfile(this);

  PrefService* local_state = g_browser_process->local_state();
  ssl_config_service_manager_.reset(
      SSLConfigServiceManager::CreateDefaultManager(local_state));

  // Initialize the BackgroundModeManager - this has to be done here before
  // InitExtensions() is called because it relies on receiving notifications
  // when extensions are loaded. BackgroundModeManager is not needed under
  // ChromeOS because Chrome is always running, no need for special keep-alive
  // or launch-on-startup support unless kKeepAliveForTest is set.
  bool init_background_mode_manager = true;
#if defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kKeepAliveForTest))
    init_background_mode_manager = false;
#endif
  if (init_background_mode_manager) {
    if (g_browser_process->background_mode_manager())
      g_browser_process->background_mode_manager()->RegisterProfile(this);
  }

  base::FilePath cookie_path = GetPath();
  cookie_path = cookie_path.Append(chrome::kCookieFilename);
  base::FilePath channel_id_path = GetPath();
  channel_id_path = channel_id_path.Append(chrome::kChannelIDFilename);
  base::FilePath cache_path = base_cache_path_;
  int cache_max_size;
  GetCacheParameters(false, &cache_path, &cache_max_size);
  cache_path = GetCachePath(cache_path);

  base::FilePath media_cache_path = base_cache_path_;
  int media_cache_max_size;
  GetCacheParameters(true, &media_cache_path, &media_cache_max_size);
  media_cache_path = GetMediaCachePath(media_cache_path);

  base::FilePath extensions_cookie_path = GetPath();
  extensions_cookie_path =
      extensions_cookie_path.Append(chrome::kExtensionsCookieFilename);

  base::FilePath infinite_cache_path = GetPath();
  infinite_cache_path =
      infinite_cache_path.Append(FILE_PATH_LITERAL("Infinite Cache"));

#if defined(OS_ANDROID)
  SessionStartupPref::Type startup_pref_type =
      SessionStartupPref::GetDefaultStartupType();
#else
  SessionStartupPref::Type startup_pref_type =
      StartupBrowserCreator::GetSessionStartupPref(
          *CommandLine::ForCurrentProcess(), this).type;
#endif
  content::CookieStoreConfig::SessionCookieMode session_cookie_mode =
      content::CookieStoreConfig::PERSISTANT_SESSION_COOKIES;
  if (GetLastSessionExitType() == Profile::EXIT_CRASHED ||
      startup_pref_type == SessionStartupPref::LAST) {
    session_cookie_mode = content::CookieStoreConfig::RESTORED_SESSION_COOKIES;
  }

  InitHostZoomMap();

  base::Callback<void(bool)> data_reduction_proxy_unavailable;
  scoped_ptr<data_reduction_proxy::DataReductionProxyParams>
      data_reduction_proxy_params;
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  DataReductionProxyChromeSettings* data_reduction_proxy_chrome_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(this);
  data_reduction_proxy_params =
      data_reduction_proxy_chrome_settings->params()->Clone();
  data_reduction_proxy_unavailable =
      base::Bind(
          &data_reduction_proxy::DataReductionProxySettings::SetUnreachable,
          base::Unretained(data_reduction_proxy_chrome_settings));
#endif

  // Make sure we initialize the ProfileIOData after everything else has been
  // initialized that we might be reading from the IO thread.

  io_data_.Init(cookie_path, channel_id_path, cache_path,
                cache_max_size, media_cache_path, media_cache_max_size,
                extensions_cookie_path, GetPath(), infinite_cache_path,
                predictor_, session_cookie_mode, GetSpecialStoragePolicy(),
                CreateDomainReliabilityMonitor(),
                data_reduction_proxy_unavailable,
                data_reduction_proxy_params.Pass());

#if defined(SPDY_PROXY_AUTH_ORIGIN)
  scoped_ptr<data_reduction_proxy::DataReductionProxyConfigurator>
      configurator(new DataReductionProxyChromeConfigurator(prefs_.get()));
  data_reduction_proxy_chrome_settings->InitDataReductionProxySettings(
      configurator.Pass(),
      prefs_.get(),
      g_browser_process->local_state(),
      GetRequestContext());
#endif

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      PluginPrefs::GetForProfile(this).get(),
      io_data_.GetResourceContextNoInit());
#endif

  // Delay README creation to not impact startup performance.
  BrowserThread::PostDelayedTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&EnsureReadmeFile, GetPath()),
        base::TimeDelta::FromMilliseconds(create_readme_delay_ms));

  TRACE_EVENT0("browser", "ProfileImpl::SetSaveSessionStorageOnDisk");
  content::BrowserContext::GetDefaultStoragePartition(this)->
      GetDOMStorageContext()->SetSaveSessionStorageOnDisk();

  // The DomDistillerViewerSource is not a normal WebUI so it must be registered
  // as a URLDataSource early.
  RegisterDomDistillerViewerSource(this);

#if !defined(OS_ANDROID)
  BrowserThread::GetBlockingPool()->PostDelayedWorkerTask(
      FROM_HERE,
      base::Bind(&DeleteResetPromptMementoFile, GetPath()),
      base::TimeDelta::FromMilliseconds(2 * create_readme_delay_ms));
#endif

  // Creation has been finished.
  TRACE_EVENT_END1("browser",
                   "Profile::CreateProfile",
                   "profile_path",
                   path_.value().c_str());

#if defined(OS_CHROMEOS)
  if (chromeos::LoginUtils::Get()->RestartToApplyPerSessionFlagsIfNeed(this,
                                                                       true)) {
    return;
  }
#endif

  if (delegate_) {
    TRACE_EVENT0("browser", "ProfileImpl::DoFileInit:DelegateOnProfileCreated")
    delegate_->OnProfileCreated(this, true, IsNewProfile());
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(this),
      content::NotificationService::NoDetails());

#if !defined(OS_CHROMEOS)
  // Listen for bookmark model load, to bootstrap the sync service.
  // On CrOS sync service will be initialized after sign in.
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(this);
  model->AddObserver(new BookmarkModelLoadedObserver(this));
#endif

  gcm::PushMessagingServiceImpl::InitializeForProfile(this);
}

void ProfileImpl::InitHostZoomMap() {
  HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);
  host_zoom_map->SetDefaultZoomLevel(
      prefs_->GetDouble(prefs::kDefaultZoomLevel));

  const base::DictionaryValue* host_zoom_dictionary =
      prefs_->GetDictionary(prefs::kPerHostZoomLevels);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (host_zoom_dictionary != NULL) {
    std::vector<std::string> keys_to_remove;
    for (base::DictionaryValue::Iterator i(*host_zoom_dictionary); !i.IsAtEnd();
         i.Advance()) {
      const std::string& host(i.key());
      double zoom_level = 0;

      bool success = i.value().GetAsDouble(&zoom_level);
      DCHECK(success);

      // Filter out A) the empty host, B) zoom levels equal to the default; and
      // remember them, so that we can later erase them from Prefs.
      // Values of type A and B could have been stored due to crbug.com/364399.
      // Values of type B could further have been stored before the default zoom
      // level was set to its current value. In either case, SetZoomLevelForHost
      // will ignore type B values, thus, to have consistency with HostZoomMap's
      // internal state, these values must also be removed from Prefs.
      if (host.empty() ||
          content::ZoomValuesEqual(zoom_level,
                                   host_zoom_map->GetDefaultZoomLevel())) {
        keys_to_remove.push_back(host);
        continue;
      }

      host_zoom_map->SetZoomLevelForHost(host, zoom_level);
    }

    DictionaryPrefUpdate update(prefs_.get(), prefs::kPerHostZoomLevels);
    base::DictionaryValue* host_zoom_dictionary = update.Get();
    for (std::vector<std::string>::const_iterator it = keys_to_remove.begin();
         it != keys_to_remove.end(); ++it) {
      host_zoom_dictionary->RemoveWithoutPathExpansion(*it, NULL);
    }
  }

  zoom_subscription_ = host_zoom_map->AddZoomLevelChangedCallback(
      base::Bind(&ProfileImpl::OnZoomLevelChanged, base::Unretained(this)));
}

base::FilePath ProfileImpl::last_selected_directory() {
  return GetPrefs()->GetFilePath(prefs::kSelectFileLastDirectory);
}

void ProfileImpl::set_last_selected_directory(const base::FilePath& path) {
  GetPrefs()->SetFilePath(prefs::kSelectFileLastDirectory, path);
}

ProfileImpl::~ProfileImpl() {
  MaybeSendDestroyedNotification();

  bool prefs_loaded = prefs_->GetInitializationStatus() !=
      PrefService::INITIALIZATION_STATUS_WAITING;

#if defined(ENABLE_SESSION_SERVICE)
  StopCreateSessionServiceTimer();
#endif

  // Remove pref observers
  pref_change_registrar_.RemoveAll();

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
      io_data_.GetResourceContextNoInit());
#endif

  // Destroy OTR profile and its profile services first.
  if (off_the_record_profile_) {
    ProfileDestroyer::DestroyOffTheRecordProfileNow(
        off_the_record_profile_.get());
  } else {
#if defined(ENABLE_EXTENSIONS)
    ExtensionPrefValueMapFactory::GetForBrowserContext(this)->
        ClearAllIncognitoSessionOnlyPreferences();
#endif
  }

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);

  if (top_sites_.get())
    top_sites_->Shutdown();

  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();

  if (host_content_settings_map_.get())
    host_content_settings_map_->ShutdownOnUIThread();

  // This causes the Preferences file to be written to disk.
  if (prefs_loaded)
    SetExitType(EXIT_NORMAL);
}

std::string ProfileImpl::GetProfileName() {
  return GetPrefs()->GetString(prefs::kGoogleServicesUsername);
}

Profile::ProfileType ProfileImpl::GetProfileType() const {
  return REGULAR_PROFILE;
}

base::FilePath ProfileImpl::GetPath() const {
  return path_;
}

scoped_refptr<base::SequencedTaskRunner> ProfileImpl::GetIOTaskRunner() {
  return JsonPrefStore::GetTaskRunnerForFile(
      GetPath(), BrowserThread::GetBlockingPool());
}

bool ProfileImpl::IsOffTheRecord() const {
  return false;
}

Profile* ProfileImpl::GetOffTheRecordProfile() {
  if (!off_the_record_profile_) {
    scoped_ptr<Profile> p(CreateOffTheRecordProfile());
    off_the_record_profile_.swap(p);

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::Source<Profile>(off_the_record_profile_.get()),
        content::NotificationService::NoDetails());
  }
  return off_the_record_profile_.get();
}

void ProfileImpl::DestroyOffTheRecordProfile() {
  off_the_record_profile_.reset();
#if defined(ENABLE_EXTENSIONS)
  ExtensionPrefValueMapFactory::GetForBrowserContext(this)->
      ClearAllIncognitoSessionOnlyPreferences();
#endif
}

bool ProfileImpl::HasOffTheRecordProfile() {
  return off_the_record_profile_.get() != NULL;
}

Profile* ProfileImpl::GetOriginalProfile() {
  return this;
}

bool ProfileImpl::IsSupervised() {
  return !GetPrefs()->GetString(prefs::kSupervisedUserId).empty();
}

ExtensionSpecialStoragePolicy*
    ProfileImpl::GetExtensionSpecialStoragePolicy() {
#if defined(ENABLE_EXTENSIONS)
  if (!extension_special_storage_policy_.get()) {
    TRACE_EVENT0("browser", "ProfileImpl::GetExtensionSpecialStoragePolicy")
    extension_special_storage_policy_ = new ExtensionSpecialStoragePolicy(
        CookieSettings::Factory::GetForProfile(this).get());
  }
  return extension_special_storage_policy_.get();
#else
  return NULL;
#endif
}

void ProfileImpl::OnPrefsLoaded(bool success) {
  TRACE_EVENT0("browser", "ProfileImpl::OnPrefsLoaded")
  if (!success) {
    if (delegate_)
      delegate_->OnProfileCreated(this, false, false);
    return;
  }

  // TODO(mirandac): remove migration code after 6 months (crbug.com/69995).
  if (g_browser_process->local_state())
    chrome::MigrateBrowserPrefs(this, g_browser_process->local_state());
  // TODO(ivankr): remove cleanup code eventually (crbug.com/165672).
  chrome::MigrateUserPrefs(this);

  // |kSessionExitType| was added after |kSessionExitedCleanly|. If the pref
  // value is empty fallback to checking for |kSessionExitedCleanly|.
  const std::string exit_type_pref_value(
      prefs_->GetString(prefs::kSessionExitType));
  if (exit_type_pref_value.empty()) {
    last_session_exit_type_ =
        prefs_->GetBoolean(prefs::kSessionExitedCleanly) ?
          EXIT_NORMAL : EXIT_CRASHED;
  } else {
    last_session_exit_type_ =
        SessionTypePrefValueToExitType(exit_type_pref_value);
  }
  // Mark the session as open.
  prefs_->SetString(prefs::kSessionExitType, kPrefExitTypeCrashed);
  // Force this to true in case we fallback and use it.
  // TODO(sky): remove this in a couple of releases (m28ish).
  prefs_->SetBoolean(prefs::kSessionExitedCleanly, true);

#if defined(OS_ANDROID) && defined(FULL_SAFE_BROWSING)
  // Clear safe browsing setting in the case we need to roll back
  // for users enrolled in Finch trial before.
  if (!SafeBrowsingService::IsEnabledByFieldTrial())
    prefs_->ClearPref(prefs::kSafeBrowsingEnabled);
#endif

  g_browser_process->profile_manager()->InitProfileUserPrefs(this);

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  DCHECK(!net_pref_observer_);
  {
    TRACE_EVENT0("browser", "ProfileImpl::OnPrefsLoaded:NetPrefObserver")
    net_pref_observer_.reset(new NetPrefObserver(
        prefs_.get(),
        prerender::PrerenderManagerFactory::GetForProfile(this),
        predictor_));
  }

  chrome_prefs::SchedulePrefsFilePathVerification(path_);

  ChromeVersionService::OnProfileLoaded(prefs_.get(), IsNewProfile());
  DoFinalInit();
}

bool ProfileImpl::WasCreatedByVersionOrLater(const std::string& version) {
  Version profile_version(ChromeVersionService::GetVersion(prefs_.get()));
  Version arg_version(version);
  return (profile_version.CompareTo(arg_version) >= 0);
}

void ProfileImpl::SetExitType(ExitType exit_type) {
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(this))
    return;
#endif
  if (!prefs_)
    return;
  ExitType current_exit_type = SessionTypePrefValueToExitType(
      prefs_->GetString(prefs::kSessionExitType));
  // This may be invoked multiple times during shutdown. Only persist the value
  // first passed in (unless it's a reset to the crash state, which happens when
  // foregrounding the app on mobile).
  if (exit_type == EXIT_CRASHED || current_exit_type == EXIT_CRASHED) {
    prefs_->SetString(prefs::kSessionExitType,
                      ExitTypeToSessionTypePrefValue(exit_type));

    // NOTE: If you change what thread this writes on, be sure and update
    // chrome::SessionEnding().
    prefs_->CommitPendingWrite();
  }
}

Profile::ExitType ProfileImpl::GetLastSessionExitType() {
  // last_session_exited_cleanly_ is set when the preferences are loaded. Force
  // it to be set by asking for the prefs.
  GetPrefs();
  return last_session_exit_type_;
}

PrefService* ProfileImpl::GetPrefs() {
  DCHECK(prefs_);  // Should explicitly be initialized.
  return prefs_.get();
}

PrefService* ProfileImpl::GetOffTheRecordPrefs() {
  DCHECK(prefs_);
  if (!otr_prefs_) {
    // The new ExtensionPrefStore is ref_counted and the new PrefService
    // stores a reference so that we do not leak memory here.
    otr_prefs_.reset(prefs_->CreateIncognitoPrefService(
        CreateExtensionPrefStore(this, true)));
  }
  return otr_prefs_.get();
}

net::URLRequestContextGetter* ProfileImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_.CreateMainRequestContextGetter(
      protocol_handlers,
      request_interceptors.Pass(),
      g_browser_process->local_state(),
      g_browser_process->io_thread()).get();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  content::RenderProcessHost* rph = content::RenderProcessHost::FromID(
      renderer_child_id);

  return rph->GetStoragePartition()->GetURLRequestContext();
}

net::URLRequestContextGetter* ProfileImpl::GetMediaRequestContext() {
  // Return the default media context.
  return io_data_.GetMediaRequestContextGetter().get();
}

net::URLRequestContextGetter*
ProfileImpl::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  content::RenderProcessHost* rph = content::RenderProcessHost::FromID(
      renderer_child_id);
  content::StoragePartition* storage_partition = rph->GetStoragePartition();

  return storage_partition->GetMediaURLRequestContext();
}

net::URLRequestContextGetter*
ProfileImpl::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return io_data_
      .GetIsolatedMediaRequestContextGetter(partition_path, in_memory).get();
}

content::ResourceContext* ProfileImpl::GetResourceContext() {
  return io_data_.GetResourceContext();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContextForExtensions() {
  return io_data_.GetExtensionsRequestContextGetter().get();
}

net::URLRequestContextGetter*
ProfileImpl::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_.CreateIsolatedAppRequestContextGetter(
      partition_path,
      in_memory,
      protocol_handlers,
      request_interceptors.Pass()).get();
}

net::SSLConfigService* ProfileImpl::GetSSLConfigService() {
  // If ssl_config_service_manager_ is null, this typically means that some
  // KeyedService is trying to create a RequestContext at startup,
  // but SSLConfigServiceManager is not initialized until DoFinalInit() which is
  // invoked after all KeyedServices have been initialized (see
  // http://crbug.com/171406).
  DCHECK(ssl_config_service_manager_) <<
      "SSLConfigServiceManager is not initialized yet";
  return ssl_config_service_manager_->Get();
}

HostContentSettingsMap* ProfileImpl::GetHostContentSettingsMap() {
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(GetPrefs(), false);
  }
  return host_content_settings_map_.get();
}

content::BrowserPluginGuestManager* ProfileImpl::GetGuestManager() {
#if defined(ENABLE_EXTENSIONS)
  return extensions::GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

DownloadManagerDelegate* ProfileImpl::GetDownloadManagerDelegate() {
  return DownloadServiceFactory::GetForBrowserContext(this)->
      GetDownloadManagerDelegate();
}

quota::SpecialStoragePolicy* ProfileImpl::GetSpecialStoragePolicy() {
#if defined(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

content::PushMessagingService* ProfileImpl::GetPushMessagingService() {
  return gcm::GCMProfileServiceFactory::GetForProfile(
      this)->push_messaging_service();
}

content::SSLHostStateDelegate* ProfileImpl::GetSSLHostStateDelegate() {
  return ChromeSSLHostStateDelegateFactory::GetForProfile(this);
}

bool ProfileImpl::IsSameProfile(Profile* profile) {
  if (profile == static_cast<Profile*>(this))
    return true;
  Profile* otr_profile = off_the_record_profile_.get();
  return otr_profile && profile == otr_profile;
}

Time ProfileImpl::GetStartTime() const {
  return start_time_;
}

history::TopSites* ProfileImpl::GetTopSites() {
  if (!top_sites_.get()) {
    top_sites_ = history::TopSites::Create(
        this, GetPath().Append(chrome::kTopSitesFilename));
  }
  return top_sites_.get();
}

history::TopSites* ProfileImpl::GetTopSitesWithoutCreating() {
  return top_sites_.get();
}

void ProfileImpl::OnDefaultZoomLevelChanged() {
  HostZoomMap::GetForBrowserContext(this)->SetDefaultZoomLevel(
      pref_change_registrar_.prefs()->GetDouble(prefs::kDefaultZoomLevel));
}

void ProfileImpl::OnZoomLevelChanged(
    const HostZoomMap::ZoomLevelChange& change) {

  if (change.mode != HostZoomMap::ZOOM_CHANGED_FOR_HOST)
    return;
  HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);
  double level = change.zoom_level;
  DictionaryPrefUpdate update(prefs_.get(), prefs::kPerHostZoomLevels);
  base::DictionaryValue* host_zoom_dictionary = update.Get();
  if (content::ZoomValuesEqual(level, host_zoom_map->GetDefaultZoomLevel()))
    host_zoom_dictionary->RemoveWithoutPathExpansion(change.host, NULL);
  else
    host_zoom_dictionary->SetDoubleWithoutPathExpansion(change.host, level);
}

#if defined(ENABLE_SESSION_SERVICE)
void ProfileImpl::StopCreateSessionServiceTimer() {
  create_session_service_timer_.Stop();
}

void ProfileImpl::EnsureSessionServiceCreated() {
  SessionServiceFactory::GetForProfile(this);
}
#endif

#if defined(OS_CHROMEOS)
void ProfileImpl::ChangeAppLocale(
    const std::string& new_locale, AppLocaleChangedVia via) {
  if (new_locale.empty()) {
    NOTREACHED();
    return;
  }
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  if (local_state->IsManagedPreference(prefs::kApplicationLocale))
    return;
  std::string pref_locale = GetPrefs()->GetString(prefs::kApplicationLocale);
  bool do_update_pref = true;
  switch (via) {
    case APP_LOCALE_CHANGED_VIA_SETTINGS:
    case APP_LOCALE_CHANGED_VIA_REVERT: {
      // We keep kApplicationLocaleBackup value as a reference.  In case value
      // of kApplicationLocale preference would change due to sync from other
      // device then kApplicationLocaleBackup value will trigger and allow us to
      // show notification about automatic locale change in LocaleChangeGuard.
      GetPrefs()->SetString(prefs::kApplicationLocaleBackup, new_locale);
      GetPrefs()->ClearPref(prefs::kApplicationLocaleAccepted);
      // We maintain kApplicationLocale property in both a global storage
      // and user's profile.  Global property determines locale of login screen,
      // while user's profile determines his personal locale preference.
      break;
    }
    case APP_LOCALE_CHANGED_VIA_LOGIN:
    case APP_LOCALE_CHANGED_VIA_PUBLIC_SESSION_LOGIN: {
      if (!pref_locale.empty()) {
        DCHECK(pref_locale == new_locale);
        std::string accepted_locale =
            GetPrefs()->GetString(prefs::kApplicationLocaleAccepted);
        if (accepted_locale == new_locale) {
          // If locale is accepted then we do not want to show LocaleChange
          // notification.  This notification is triggered by different values
          // of kApplicationLocaleBackup and kApplicationLocale preferences,
          // so make them identical.
          GetPrefs()->SetString(prefs::kApplicationLocaleBackup, new_locale);
        } else {
          // Back up locale of login screen.
          std::string cur_locale = g_browser_process->GetApplicationLocale();
          GetPrefs()->SetString(prefs::kApplicationLocaleBackup, cur_locale);
          if (locale_change_guard_ == NULL)
            locale_change_guard_.reset(new chromeos::LocaleChangeGuard(this));
          locale_change_guard_->PrepareChangingLocale(cur_locale, new_locale);
        }
      } else {
        std::string cur_locale = g_browser_process->GetApplicationLocale();
        std::string backup_locale =
            GetPrefs()->GetString(prefs::kApplicationLocaleBackup);
        // Profile synchronization takes time and is not completed at that
        // moment at first login.  So we initialize locale preference in steps:
        // (1) first save it to temporary backup;
        // (2) on next login we assume that synchronization is already completed
        //     and we may finalize initialization.
        GetPrefs()->SetString(prefs::kApplicationLocaleBackup, cur_locale);
        if (!new_locale.empty())
          GetPrefs()->SetString(prefs::kApplicationLocale, new_locale);
        else if (!backup_locale.empty())
          GetPrefs()->SetString(prefs::kApplicationLocale, backup_locale);
        do_update_pref = false;
      }
      break;
    }
    case APP_LOCALE_CHANGED_VIA_UNKNOWN:
    default: {
      NOTREACHED();
      break;
    }
  }
  if (do_update_pref)
    GetPrefs()->SetString(prefs::kApplicationLocale, new_locale);
  if (via != APP_LOCALE_CHANGED_VIA_PUBLIC_SESSION_LOGIN)
    local_state->SetString(prefs::kApplicationLocale, new_locale);

  if (chromeos::UserManager::Get()->GetOwnerEmail() ==
      chromeos::ProfileHelper::Get()->GetUserByProfile(this)->email())
    local_state->SetString(prefs::kOwnerLocale, new_locale);
}

void ProfileImpl::OnLogin() {
  if (locale_change_guard_ == NULL)
    locale_change_guard_.reset(new chromeos::LocaleChangeGuard(this));
  locale_change_guard_->OnLogin();
}

void ProfileImpl::InitChromeOSPreferences() {
  chromeos_preferences_.reset(new chromeos::Preferences());
  chromeos_preferences_->Init(
      PrefServiceSyncable::FromProfile(this),
      chromeos::ProfileHelper::Get()->GetUserByProfile(this));
}

#endif  // defined(OS_CHROMEOS)

PrefProxyConfigTracker* ProfileImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_)
    pref_proxy_config_tracker_.reset(CreateProxyConfigTracker());
  return pref_proxy_config_tracker_.get();
}

chrome_browser_net::Predictor* ProfileImpl::GetNetworkPredictor() {
  return predictor_;
}

DevToolsNetworkController* ProfileImpl::GetDevToolsNetworkController() {
  return io_data_.GetDevToolsNetworkController();
}

void ProfileImpl::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  io_data_.ClearNetworkingHistorySince(time, completion);
}

GURL ProfileImpl::GetHomePage() {
  // --homepage overrides any preferences.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kHomePage)) {
    // TODO(evanm): clean up usage of DIR_CURRENT.
    //   http://code.google.com/p/chromium/issues/detail?id=60630
    // For now, allow this code to call getcwd().
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    base::FilePath browser_directory;
    PathService::Get(base::DIR_CURRENT, &browser_directory);
    GURL home_page(url_fixer::FixupRelativeFile(
        browser_directory,
        command_line.GetSwitchValuePath(switches::kHomePage)));
    if (home_page.is_valid())
      return home_page;
  }

  if (GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
    return GURL(chrome::kChromeUINewTabURL);
  GURL home_page(url_fixer::FixupURL(GetPrefs()->GetString(prefs::kHomePage),
                                     std::string()));
  if (!home_page.is_valid())
    return GURL(chrome::kChromeUINewTabURL);
  return home_page;
}

void ProfileImpl::UpdateProfileUserNameCache() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(GetPath());
  if (index != std::string::npos) {
    std::string user_name =
        GetPrefs()->GetString(prefs::kGoogleServicesUsername);
    cache.SetUserNameOfProfileAtIndex(index, base::UTF8ToUTF16(user_name));
    ProfileMetrics::UpdateReportedProfilesStatistics(profile_manager);
  }
}

void ProfileImpl::UpdateProfileSupervisedUserIdCache() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(GetPath());
  if (index != std::string::npos) {
    std::string supervised_user_id =
        GetPrefs()->GetString(prefs::kSupervisedUserId);
    cache.SetSupervisedUserIdOfProfileAtIndex(index, supervised_user_id);
    ProfileMetrics::UpdateReportedProfilesStatistics(profile_manager);
  }
}

void ProfileImpl::UpdateProfileNameCache() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(GetPath());
  if (index != std::string::npos) {
    std::string profile_name =
        GetPrefs()->GetString(prefs::kProfileName);
    cache.SetNameOfProfileAtIndex(index, base::UTF8ToUTF16(profile_name));
  }
}

void ProfileImpl::UpdateProfileAvatarCache() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(GetPath());
  if (index != std::string::npos) {
    size_t avatar_index =
        GetPrefs()->GetInteger(prefs::kProfileAvatarIndex);
    cache.SetAvatarIconOfProfileAtIndex(index, avatar_index);
  }
}

void ProfileImpl::UpdateProfileIsEphemeralCache() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(GetPath());
  if (index != std::string::npos) {
    bool is_ephemeral = GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles);
    cache.SetProfileIsEphemeralAtIndex(index, is_ephemeral);
  }
}

// Gets the cache parameters from the command line. If |is_media_context| is
// set to true then settings for the media context type is what we need,
// |cache_path| will be set to the user provided path, or will not be touched if
// there is not an argument. |max_size| will be the user provided value or zero
// by default.
void ProfileImpl::GetCacheParameters(bool is_media_context,
                                     base::FilePath* cache_path,
                                     int* max_size) {
  DCHECK(cache_path);
  DCHECK(max_size);
  base::FilePath path(prefs_->GetFilePath(prefs::kDiskCacheDir));
  if (!path.empty())
    *cache_path = path;
  *max_size = is_media_context ? prefs_->GetInteger(prefs::kMediaCacheSize) :
                                 prefs_->GetInteger(prefs::kDiskCacheSize);
}

PrefProxyConfigTracker* ProfileImpl::CreateProxyConfigTracker() {
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(this)) {
    return ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
        g_browser_process->local_state());
  }
#endif  // defined(OS_CHROMEOS)
  return ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
      GetPrefs(), g_browser_process->local_state());
}

scoped_ptr<domain_reliability::DomainReliabilityMonitor>
ProfileImpl::CreateDomainReliabilityMonitor() {
  domain_reliability::DomainReliabilityService* service =
      domain_reliability::DomainReliabilityServiceFactory::GetInstance()->
          GetForBrowserContext(this);
  if (!service)
    return scoped_ptr<domain_reliability::DomainReliabilityMonitor>();

  return service->CreateMonitor(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
}
