// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background_sync/background_sync_controller_factory.h"
#include "chrome/browser/background_sync/background_sync_controller_impl.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dom_distiller/profile_utils.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/domain_reliability/monitor.h"
#include "components/domain_reliability/service.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/ssl_config/ssl_config_service_manager.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/ui/zoom/zoom_event_manager.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/tracked/tracked_preference_validation_delegate.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/page_zoom.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/locale_change_guard.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(ENABLE_BACKGROUND)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_system.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/content_settings/content_settings_supervised_provider.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

using base::Time;
using base::TimeDelta;
using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using content::BrowserThread;
using content::DownloadManagerDelegate;

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

void CreateProfileReadme(const base::FilePath& profile_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::FilePath readme_path = profile_path.Append(chrome::kReadmeFilename);
  std::string product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  std::string readme_text = base::StringPrintf(
      kReadmeText, product_name.c_str(), product_name.c_str());
  if (base::WriteFile(readme_path, readme_text.data(), readme_text.size()) ==
      -1) {
    LOG(ERROR) << "Could not create README file.";
  }
}

// Helper method needed because PostTask cannot currently take a Callback
// function with non-void return type.
void CreateDirectoryAndSignal(const base::FilePath& path,
                              base::WaitableEvent* done_creating,
                              bool create_readme) {
  // If the readme exists, the profile directory must also already exist.
  base::FilePath readme_path = path.Append(chrome::kReadmeFilename);
  if (base::PathExists(readme_path)) {
    done_creating->Signal();
    return;
  }

  DVLOG(1) << "Creating directory " << path.value();
  if (base::CreateDirectory(path) && create_readme)
    CreateProfileReadme(path);
  done_creating->Signal();
}

// Task that blocks the FILE thread until CreateDirectoryAndSignal() finishes on
// blocking I/O pool.
void BlockFileThreadOnDirectoryCreate(base::WaitableEvent* done_creating) {
  done_creating->Wait();
}

// Initiates creation of profile directory on |sequenced_task_runner| and
// ensures that FILE thread is blocked until that operation finishes. If
// |create_readme| is true, the profile README will be created in the profile
// directory.
void CreateProfileDirectory(base::SequencedTaskRunner* sequenced_task_runner,
                            const base::FilePath& path,
                            bool create_readme) {
  base::WaitableEvent* done_creating = new base::WaitableEvent(false, false);
  sequenced_task_runner->PostTask(
      FROM_HERE, base::Bind(&CreateDirectoryAndSignal, path, done_creating,
                            create_readme));
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

}  // namespace

// static
Profile* Profile::CreateProfile(const base::FilePath& path,
                                Delegate* delegate,
                                CreateMode create_mode) {
  TRACE_EVENT1("browser,startup",
               "Profile::CreateProfile",
               "profile_path",
               path.AsUTF8Unsafe());

  // Get sequenced task runner for making sure that file operations of
  // this profile (defined by |path|) are executed in expected order
  // (what was previously assured by the FILE thread).
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(path,
                                          BrowserThread::GetBlockingPool());
  if (create_mode == CREATE_MODE_ASYNCHRONOUS) {
    DCHECK(delegate);
    CreateProfileDirectory(sequenced_task_runner.get(), path, true);
  } else if (create_mode == CREATE_MODE_SYNCHRONOUS) {
    if (!base::PathExists(path)) {
      // TODO(rogerta): http://crbug/160553 - Bad things happen if we can't
      // write to the profile directory.  We should eventually be able to run in
      // this situation.
      if (!base::CreateDirectory(path))
        return NULL;

      CreateProfileReadme(path);
    }
  } else {
    NOTREACHED();
  }

  return new ProfileImpl(
      path, delegate, create_mode, sequenced_task_runner.get());
}

// static
const char ProfileImpl::kPrefExitTypeNormal[] = "Normal";

// static
void ProfileImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled, false);
  registry->RegisterBooleanPref(prefs::kAllowDeletingBrowserHistory, true);
  registry->RegisterBooleanPref(prefs::kForceGoogleSafeSearch, false);
  registry->RegisterBooleanPref(prefs::kForceYouTubeSafetyMode, false);
  registry->RegisterBooleanPref(prefs::kRecordHistory, false);
  registry->RegisterIntegerPref(
      prefs::kProfileAvatarIndex,
      -1,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // Whether a profile is using an avatar without having explicitely chosen it
  // (i.e. was assigned by default by legacy profile creation).
  registry->RegisterBooleanPref(
      prefs::kProfileUsingDefaultAvatar,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kProfileUsingGAIAAvatar,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // Whether a profile is using a default avatar name (eg. Pickles or Person 1).
  registry->RegisterBooleanPref(
      prefs::kProfileUsingDefaultName,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kSupervisedUserId, std::string());
  registry->RegisterStringPref(prefs::kProfileName,
                               std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kHomePage,
                               std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(ENABLE_PRINTING)
  registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);
#endif
  registry->RegisterBooleanPref(prefs::kPrintPreviewDisabled, false);
  registry->RegisterStringPref(
      prefs::kPrintPreviewDefaultDestinationSelectionRules, std::string());
  registry->RegisterBooleanPref(prefs::kForceEphemeralProfiles, false);

  // Initialize the cache prefs.
  registry->RegisterFilePathPref(prefs::kDiskCacheDir, base::FilePath());
  registry->RegisterIntegerPref(prefs::kDiskCacheSize, 0);
  registry->RegisterIntegerPref(prefs::kMediaCacheSize, 0);
}

ProfileImpl::ProfileImpl(
    const base::FilePath& path,
    Delegate* delegate,
    CreateMode create_mode,
    base::SequencedTaskRunner* sequenced_task_runner)
    : path_(path),
      pref_registry_(new user_prefs::PrefRegistrySyncable),
      io_data_(this),
      last_session_exit_type_(EXIT_NORMAL),
      start_time_(Time::Now()),
      delegate_(delegate),
      predictor_(NULL) {
  TRACE_EVENT0("browser,startup", "ProfileImpl::ctor")
  DCHECK(!path.empty()) << "Using an empty path will attempt to write " <<
                            "profile files to the root directory!";

#if defined(ENABLE_SESSION_SERVICE)
  create_session_service_timer_.Start(FROM_HERE,
      TimeDelta::FromMilliseconds(kCreateSessionServiceDelayMS), this,
      &ProfileImpl::EnsureSessionServiceCreated);
#endif

  set_is_guest_profile(path == ProfileManager::GetGuestProfilePath());
  set_is_system_profile(path == ProfileManager::GetSystemProfilePath());

  // Determine if prefetch is enabled for this profile.
  // If not profile_manager is present, it means we are in a unittest.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  predictor_ = chrome_browser_net::Predictor::CreatePredictor(
      !command_line->HasSwitch(switches::kDisablePreconnect),
      !command_line->HasSwitch(switches::kDnsPrefetchDisable),
      g_browser_process->profile_manager() == NULL);

  // If we are creating the profile synchronously, then we should load the
  // policy data immediately.
  bool force_immediate_policy_load = (create_mode == CREATE_MODE_SYNCHRONOUS);
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
  profile_policy_connector_ =
      policy::ProfilePolicyConnectorFactory::CreateForBrowserContext(
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

  SupervisedUserSettingsService* supervised_user_settings = nullptr;
#if defined(ENABLE_SUPERVISED_USERS)
  supervised_user_settings =
      SupervisedUserSettingsServiceFactory::GetForProfile(this);
  supervised_user_settings->Init(
      path_, sequenced_task_runner, create_mode == CREATE_MODE_SYNCHRONOUS);
#endif

  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());
  if (safe_browsing_service.get()) {
    pref_validation_delegate_ =
        safe_browsing_service->CreatePreferenceValidationDelegate(this);
  }

  content::BrowserContext::Initialize(this, path_);

  {
    prefs_ = chrome_prefs::CreateProfilePrefs(
        path_, sequenced_task_runner, pref_validation_delegate_.get(),
        profile_policy_connector_->policy_service(), supervised_user_settings,
        CreateExtensionPrefStore(this, false), pref_registry_, async_prefs);
    // Register on BrowserContext.
    user_prefs::UserPrefs::Set(this, prefs_.get());
  }

  if (async_prefs) {
    // Wait for the notification that prefs has been loaded
    // (successfully or not).  Note that we can use base::Unretained
    // because the PrefService is owned by this class and lives on
    // the same thread.
    prefs_->AddPrefInitObserver(base::Bind(
        &ProfileImpl::OnPrefsLoaded, base::Unretained(this), create_mode));
  } else {
    // Prefs were loaded synchronously so we can continue directly.
    OnPrefsLoaded(create_mode, true);
  }
}

void ProfileImpl::DoFinalInit() {
  TRACE_EVENT0("browser", "ProfileImpl::DoFinalInit")
  SCOPED_UMA_HISTOGRAM_TIMER("Profile.ProfileImplDoFinalInit");

  PrefService* prefs = GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::Bind(&ProfileImpl::UpdateProfileSupervisedUserIdCache,
                 base::Unretained(this)));

  // Changes in the profile avatar.
  pref_change_registrar_.Add(
      prefs::kProfileAvatarIndex,
      base::Bind(&ProfileImpl::UpdateProfileAvatarCache,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileUsingDefaultAvatar,
      base::Bind(&ProfileImpl::UpdateProfileAvatarCache,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kProfileUsingGAIAAvatar,
      base::Bind(&ProfileImpl::UpdateProfileAvatarCache,
                 base::Unretained(this)));

  // Changes in the profile name.
  pref_change_registrar_.Add(
      prefs::kProfileUsingDefaultName,
      base::Bind(&ProfileImpl::UpdateProfileNameCache,
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
  CreateProfileDirectory(sequenced_task_runner.get(), base_cache_path_, false);

  // Initialize components that depend on the current value.
  UpdateProfileSupervisedUserIdCache();
  UpdateProfileIsEphemeralCache();
  GAIAInfoUpdateServiceFactory::GetForProfile(this);

  PrefService* local_state = g_browser_process->local_state();
  ssl_config_service_manager_.reset(
      ssl_config::SSLConfigServiceManager::CreateDefaultManager(
          local_state,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));

#if BUILDFLAG(ENABLE_BACKGROUND)
  // Initialize the BackgroundModeManager - this has to be done here before
  // InitExtensions() is called because it relies on receiving notifications
  // when extensions are loaded. BackgroundModeManager is not needed under
  // ChromeOS because Chrome is always running, no need for special keep-alive
  // or launch-on-startup support unless kKeepAliveForTest is set.
  bool init_background_mode_manager = true;
#if defined(OS_CHROMEOS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kKeepAliveForTest))
    init_background_mode_manager = false;
#endif
  if (init_background_mode_manager) {
    if (g_browser_process->background_mode_manager())
      g_browser_process->background_mode_manager()->RegisterProfile(this);
  }
#endif  // BUILDFLAG(ENABLE_BACKGROUND)

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

#if defined(OS_ANDROID)
  SessionStartupPref::Type startup_pref_type =
      SessionStartupPref::GetDefaultStartupType();
#else
  SessionStartupPref::Type startup_pref_type =
      StartupBrowserCreator::GetSessionStartupPref(
          *base::CommandLine::ForCurrentProcess(), this).type;
#endif
  content::CookieStoreConfig::SessionCookieMode session_cookie_mode =
      content::CookieStoreConfig::PERSISTANT_SESSION_COOKIES;
  if (GetLastSessionExitType() == Profile::EXIT_CRASHED ||
      startup_pref_type == SessionStartupPref::LAST) {
    session_cookie_mode = content::CookieStoreConfig::RESTORED_SESSION_COOKIES;
  }

  // Make sure we initialize the ProfileIOData after everything else has been
  // initialized that we might be reading from the IO thread.

  io_data_.Init(cookie_path, channel_id_path, cache_path,
                cache_max_size, media_cache_path, media_cache_max_size,
                extensions_cookie_path, GetPath(), predictor_,
                session_cookie_mode, GetSpecialStoragePolicy(),
                CreateDomainReliabilityMonitor(local_state));

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      PluginPrefs::GetForProfile(this).get(),
      io_data_.GetResourceContextNoInit());
#endif

  TRACE_EVENT0("browser", "ProfileImpl::SetSaveSessionStorageOnDisk");
  content::BrowserContext::GetDefaultStoragePartition(this)->
      GetDOMStorageContext()->SetSaveSessionStorageOnDisk();

  // The DomDistillerViewerSource is not a normal WebUI so it must be registered
  // as a URLDataSource early.
  RegisterDomDistillerViewerSource(this);

#if defined(OS_CHROMEOS)
  if (chromeos::UserSessionManager::GetInstance()
          ->RestartToApplyPerSessionFlagsIfNeed(this, true)) {
    return;
  }
#endif

  if (delegate_) {
    TRACE_EVENT0("browser", "ProfileImpl::DoFileInit:DelegateOnProfileCreated")
    delegate_->OnProfileCreated(this, true, IsNewProfile());
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER("Profile.NotifyProfileCreatedTime");
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::Source<Profile>(this),
        content::NotificationService::NoDetails());
  }
#if !defined(OS_CHROMEOS)
  // Listen for bookmark model load, to bootstrap the sync service.
  // On CrOS sync service will be initialized after sign in.
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(this);
  model->AddObserver(new BookmarkModelLoadedObserver(this));
#endif

  PushMessagingServiceImpl::InitializeForProfile(this);

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  signin_ui_util::InitializePrefsForProfile(this);
#endif
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

  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();

  // This causes the Preferences file to be written to disk.
  if (prefs_loaded)
    SetExitType(EXIT_NORMAL);
}

std::string ProfileImpl::GetProfileUserName() const {
  const SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(this);
  if (signin_manager)
    return signin_manager->GetAuthenticatedAccountInfo().email;

  return std::string();
}

Profile::ProfileType ProfileImpl::GetProfileType() const {
  return REGULAR_PROFILE;
}

std::unique_ptr<content::ZoomLevelDelegate>
ProfileImpl::CreateZoomLevelDelegate(const base::FilePath& partition_path) {
  return base::WrapUnique(new ChromeZoomLevelPrefs(
      GetPrefs(), GetPath(), partition_path,
      ui_zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr()));
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
    std::unique_ptr<Profile> p(CreateOffTheRecordProfile());
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
  otr_prefs_->ClearMutableValues();
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

bool ProfileImpl::IsSupervised() const {
  return !GetPrefs()->GetString(prefs::kSupervisedUserId).empty();
}

bool ProfileImpl::IsChild() const {
#if defined(ENABLE_SUPERVISED_USERS)
  return GetPrefs()->GetString(prefs::kSupervisedUserId) ==
      supervised_users::kChildAccountSUID;
#else
  return false;
#endif
}

bool ProfileImpl::IsLegacySupervised() const {
  return IsSupervised() && !IsChild();
}

ExtensionSpecialStoragePolicy*
    ProfileImpl::GetExtensionSpecialStoragePolicy() {
#if defined(ENABLE_EXTENSIONS)
  if (!extension_special_storage_policy_.get()) {
    TRACE_EVENT0("browser", "ProfileImpl::GetExtensionSpecialStoragePolicy")
    extension_special_storage_policy_ = new ExtensionSpecialStoragePolicy(
        CookieSettingsFactory::GetForProfile(this).get());
  }
  return extension_special_storage_policy_.get();
#else
  return NULL;
#endif
}

void ProfileImpl::OnLocaleReady() {
  TRACE_EVENT0("browser", "ProfileImpl::OnLocaleReady");
  SCOPED_UMA_HISTOGRAM_TIMER("Profile.OnLocaleReadyTime");
  // Migrate obsolete prefs.
  if (g_browser_process->local_state())
    chrome::MigrateObsoleteBrowserPrefs(this, g_browser_process->local_state());
  chrome::MigrateObsoleteProfilePrefs(this);

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

  g_browser_process->profile_manager()->InitProfileUserPrefs(this);

  {
    SCOPED_UMA_HISTOGRAM_TIMER("Profile.CreateBrowserContextServicesTime");
    BrowserContextDependencyManager::GetInstance()->
      CreateBrowserContextServices(this);
  }

  DCHECK(!net_pref_observer_);
  {
    TRACE_EVENT0("browser", "ProfileImpl::OnPrefsLoaded:NetPrefObserver")
    net_pref_observer_.reset(new NetPrefObserver(prefs_.get()));
  }

  ChromeVersionService::OnProfileLoaded(prefs_.get(), IsNewProfile());
  DoFinalInit();
}

void ProfileImpl::OnPrefsLoaded(CreateMode create_mode, bool success) {
  TRACE_EVENT0("browser", "ProfileImpl::OnPrefsLoaded");
  if (!success) {
    if (delegate_)
      delegate_->OnProfileCreated(this, false, false);
    return;
  }

#if defined(OS_CHROMEOS)
  if (create_mode == CREATE_MODE_SYNCHRONOUS) {
    // Synchronous create mode implies that either it is restart after crash,
    // or we are in tests. In both cases the first loaded locale is correct.
    OnLocaleReady();
  } else {
    chromeos::UserSessionManager::GetInstance()->RespectLocalePreferenceWrapper(
        this, base::Bind(&ProfileImpl::OnLocaleReady, base::Unretained(this)));
  }
#else
  OnLocaleReady();
#endif
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
  }
}

Profile::ExitType ProfileImpl::GetLastSessionExitType() {
  // last_session_exited_cleanly_ is set when the preferences are loaded. Force
  // it to be set by asking for the prefs.
  GetPrefs();
  return last_session_exit_type_;
}

PrefService* ProfileImpl::GetPrefs() {
  return const_cast<PrefService*>(
      static_cast<const ProfileImpl*>(this)->GetPrefs());
}

const PrefService* ProfileImpl::GetPrefs() const {
  DCHECK(prefs_);  // Should explicitly be initialized.
  return prefs_.get();
}

ChromeZoomLevelPrefs* ProfileImpl::GetZoomLevelPrefs() {
  return static_cast<ChromeZoomLevelPrefs*>(
      GetDefaultStoragePartition(this)->GetZoomLevelDelegate());
}

PrefService* ProfileImpl::GetOffTheRecordPrefs() {
  DCHECK(prefs_);
  if (!otr_prefs_) {
    // The new ExtensionPrefStore is ref_counted and the new PrefService
    // stores a reference so that we do not leak memory here.
    otr_prefs_.reset(CreateIncognitoPrefServiceSyncable(
        prefs_.get(), CreateExtensionPrefStore(this, true)));
  }
  return otr_prefs_.get();
}

content::ResourceContext* ProfileImpl::GetResourceContext() {
  return io_data_.GetResourceContext();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContextForExtensions() {
  return io_data_.GetExtensionsRequestContextGetter().get();
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

content::BrowserPluginGuestManager* ProfileImpl::GetGuestManager() {
#if defined(ENABLE_EXTENSIONS)
  return guest_view::GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

DownloadManagerDelegate* ProfileImpl::GetDownloadManagerDelegate() {
  return DownloadServiceFactory::GetForBrowserContext(this)->
      GetDownloadManagerDelegate();
}

storage::SpecialStoragePolicy* ProfileImpl::GetSpecialStoragePolicy() {
#if defined(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

content::PushMessagingService* ProfileImpl::GetPushMessagingService() {
  return PushMessagingServiceFactory::GetForProfile(this);
}

content::SSLHostStateDelegate* ProfileImpl::GetSSLHostStateDelegate() {
  return ChromeSSLHostStateDelegateFactory::GetForProfile(this);
}

// TODO(mlamouri): we should all these BrowserContext implementation to Profile
// instead of repeating them inside all Profile implementations.
content::PermissionManager* ProfileImpl::GetPermissionManager() {
  return PermissionManagerFactory::GetForProfile(this);
}

content::BackgroundSyncController* ProfileImpl::GetBackgroundSyncController() {
  return BackgroundSyncControllerFactory::GetForProfile(this);
}

net::URLRequestContextGetter* ProfileImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_.CreateMainRequestContextGetter(
                     protocol_handlers, std::move(request_interceptors),
                     g_browser_process->io_thread())
      .get();
}

net::URLRequestContextGetter*
ProfileImpl::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_.CreateIsolatedAppRequestContextGetter(
                     partition_path, in_memory, protocol_handlers,
                     std::move(request_interceptors))
      .get();
}

net::URLRequestContextGetter* ProfileImpl::CreateMediaRequestContext() {
  return io_data_.GetMediaRequestContextGetter().get();
}

net::URLRequestContextGetter*
ProfileImpl::CreateMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return io_data_
      .GetIsolatedMediaRequestContextGetter(partition_path, in_memory).get();
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

  if (user_manager::UserManager::Get()->GetOwnerAccountId() ==
      chromeos::ProfileHelper::Get()->GetUserByProfile(this)->GetAccountId())
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
      this, chromeos::ProfileHelper::Get()->GetUserByProfile(this));
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

DevToolsNetworkControllerHandle*
ProfileImpl::GetDevToolsNetworkControllerHandle() {
  return io_data_.GetDevToolsNetworkControllerHandle();
}

void ProfileImpl::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  io_data_.ClearNetworkingHistorySince(time, completion);
}

GURL ProfileImpl::GetHomePage() {
  // --homepage overrides any preferences.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kHomePage)) {
    // TODO(evanm): clean up usage of DIR_CURRENT.
    //   http://code.google.com/p/chromium/issues/detail?id=60630
    // For now, allow this code to call getcwd().
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    base::FilePath browser_directory;
    PathService::Get(base::DIR_CURRENT, &browser_directory);
    GURL home_page(url_formatter::FixupRelativeFile(
        browser_directory,
        command_line.GetSwitchValuePath(switches::kHomePage)));
    if (home_page.is_valid())
      return home_page;
  }

  if (GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
    return GURL(chrome::kChromeUINewTabURL);
  GURL home_page(url_formatter::FixupURL(
      GetPrefs()->GetString(prefs::kHomePage), std::string()));
  if (!home_page.is_valid())
    return GURL(chrome::kChromeUINewTabURL);
  return home_page;
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
    bool default_name =
        GetPrefs()->GetBoolean(prefs::kProfileUsingDefaultName);
    cache.SetProfileIsUsingDefaultNameAtIndex(index, default_name);
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
    bool default_avatar =
        GetPrefs()->GetBoolean(prefs::kProfileUsingDefaultAvatar);
    cache.SetProfileIsUsingDefaultAvatarAtIndex(index, default_avatar);
    bool gaia_avatar =
        GetPrefs()->GetBoolean(prefs::kProfileUsingGAIAAvatar);
    cache.SetIsUsingGAIAPictureOfProfileAtIndex(index, gaia_avatar);
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

std::unique_ptr<domain_reliability::DomainReliabilityMonitor>
ProfileImpl::CreateDomainReliabilityMonitor(PrefService* local_state) {
  domain_reliability::DomainReliabilityService* service =
      domain_reliability::DomainReliabilityServiceFactory::GetInstance()->
          GetForBrowserContext(this);
  if (!service)
    return std::unique_ptr<domain_reliability::DomainReliabilityMonitor>();

  return service->CreateMonitor(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
}
