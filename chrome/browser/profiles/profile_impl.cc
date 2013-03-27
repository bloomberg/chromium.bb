// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context_factory.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/speech/chrome_speech_recognition_preferences.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/user_style_sheet_watcher.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/startup_metric_utils.h"
#include "chrome/common/url_constants.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/browser_policy_connector.h"
#if !defined(OS_CHROMEOS)
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#endif
#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#endif
#else
#include "chrome/browser/policy/policy_service_stub.h"
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#if defined(OS_WIN)
#include "chrome/installer/util/install_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/enterprise_extension_observer.h"
#include "chrome/browser/chromeos/locale_change_guard.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#endif

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using content::DownloadManagerDelegate;
using content::HostZoomMap;
using content::UserMetricsAction;

namespace {

// Constrict us to a very specific platform and architecture to make sure
// ifdefs don't cause problems with the check.
#if defined(OS_LINUX) && defined(TOOLKIT_GTK) && defined(ARCH_CPU_X86_64) && \
  !defined(_GLIBCXX_DEBUG)
// Make sure that the ProfileImpl doesn't grow. We're currently trying to drive
// the number of services that are included in ProfileImpl (instead of using
// ProfileKeyedServiceFactory) to zero.
//
// If you don't know about this effort, please read:
//   https://sites.google.com/a/chromium.org/dev/developers/design-documents/profile-architecture
//
// REVIEWERS: Do not let anyone increment this. We need to drive the number of
// raw accessed services down to zero. DO NOT LET PEOPLE REGRESS THIS UNLESS
// THE PATCH ITSELF IS MAKING PROGRESS ON PKSF REFACTORING.
COMPILE_ASSERT(sizeof(ProfileImpl) <= 744u, profile_impl_size_unexpected);
#endif

#if defined(ENABLE_SESSION_SERVICE)
// Delay, in milliseconds, before we explicitly create the SessionService.
static const int kCreateSessionServiceDelayMS = 500;
#endif

// Text content of README file created in each profile directory. Both %s
// placeholders must contain the product name. This is not localizable and hence
// not in resources.
static const char kReadmeText[] =
    "%s settings and storage represent user-selected preferences and "
    "information and MUST not be extracted, overwritten or modified except "
    "through %s defined APIs.";

// Value written to prefs for EXIT_CRASHED and EXIT_SESSION_ENDED.
const char* const kPrefExitTypeCrashed = "Crashed";
const char* const kPrefExitTypeSessionEnded = "SessionEnded";

// Helper method needed because PostTask cannot currently take a Callback
// function with non-void return type.
void CreateDirectoryAndSignal(const base::FilePath& path,
                              base::WaitableEvent* done_creating) {
  DVLOG(1) << "Creating directory " << path.value();
  file_util::CreateDirectory(path);
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
  if (file_util::PathExists(readme_path))
    return;
  std::string product_name = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  std::string readme_text = base::StringPrintf(
      kReadmeText, product_name.c_str(), product_name.c_str());
  if (file_util::WriteFile(
          readme_path, readme_text.data(), readme_text.size()) == -1) {
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

}  // namespace

// static
Profile* Profile::CreateProfile(const base::FilePath& path,
                                Delegate* delegate,
                                CreateMode create_mode) {
  // Get sequenced task runner for making sure that file operations of
  // this profile (defined by |path|) are executed in expected order
  // (what was previously assured by the FILE thread).
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(path,
                                          BrowserThread::GetBlockingPool());
  if (create_mode == CREATE_MODE_ASYNCHRONOUS) {
    DCHECK(delegate);
    CreateProfileDirectory(sequenced_task_runner, path);
  } else if (create_mode == CREATE_MODE_SYNCHRONOUS) {
    if (!file_util::PathExists(path)) {
      // TODO(tc): http://b/1094718 Bad things happen if we can't write to the
      // profile directory.  We should eventually be able to run in this
      // situation.
      if (!file_util::CreateDirectory(path))
        return NULL;
    }
  } else {
    NOTREACHED();
  }

  return new ProfileImpl(path, delegate, create_mode, sequenced_task_runner);
}

// static
int ProfileImpl::create_readme_delay_ms = 60000;

// static
const char* const ProfileImpl::kPrefExitTypeNormal = "Normal";

// static
void ProfileImpl::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kAllowDeletingBrowserHistory,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kSigninAllowed,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kForceSafeSearch,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kProfileAvatarIndex,
                                -1,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kProfileName,
                               "",
                               PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kProfileIsManaged,
                                false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kHomePage,
                               std::string(),
                               PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(ENABLE_PRINTING)
  registry->RegisterBooleanPref(prefs::kPrintingEnabled,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(prefs::kPrintPreviewDisabled,
#if defined(GOOGLE_CHROME_BUILD)
                                false,
#else
                                true,
#endif
                                PrefRegistrySyncable::UNSYNCABLE_PREF);

  // Initialize the cache prefs.
  registry->RegisterFilePathPref(prefs::kDiskCacheDir,
                                 base::FilePath(),
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kDiskCacheSize,
                                0,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kMediaCacheSize,
                                0,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);

  // Deprecated. Kept around for migration.
  registry->RegisterBooleanPref(prefs::kClearSiteDataOnExit,
                                false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
}

ProfileImpl::ProfileImpl(
    const base::FilePath& path,
    Delegate* delegate,
    CreateMode create_mode,
    base::SequencedTaskRunner* sequenced_task_runner)
    : zoom_callback_(base::Bind(&ProfileImpl::OnZoomLevelChanged,
                                base::Unretained(this))),
      path_(path),
      pref_registry_(new PrefRegistrySyncable),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_data_(this)),
      host_content_settings_map_(NULL),
      last_session_exit_type_(EXIT_NORMAL),
      start_time_(Time::Now()),
      delegate_(delegate),
      predictor_(NULL) {
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
      g_browser_process->profile_manager() == NULL);

#if defined(ENABLE_CONFIGURATION_POLICY)
  // If we are creating the profile synchronously, then we should load the
  // policy data immediately.
  bool force_immediate_policy_load = (create_mode == CREATE_MODE_SYNCHRONOUS);

  // TODO(atwilson): Change |cloud_policy_manager_| and
  // |managed_mode_policy_provider_| to proper ProfileKeyedServices once
  // PrefServiceSyncable is a ProfileKeyedService (policy must be initialized
  // before PrefServiceSyncable because PrefServiceSyncable depends on policy
  // loading to get overridden pref values).
#if !defined(OS_CHROMEOS)
  if (!command_line->HasSwitch(switches::kDisableCloudPolicyOnSignin)) {
    cloud_policy_manager_ =
        policy::UserCloudPolicyManagerFactory::CreateForProfile(
            this, force_immediate_policy_load);
    cloud_policy_manager_->Init();
  }
#endif
#if defined(ENABLE_MANAGED_USERS)
  managed_mode_policy_provider_ =
      policy::ManagedModePolicyProvider::Create(this,
                                                sequenced_task_runner,
                                                force_immediate_policy_load);
  managed_mode_policy_provider_->Init();
#endif
  policy_service_ =
      g_browser_process->browser_policy_connector()->CreatePolicyService(this);
#else
  policy_service_.reset(new policy::PolicyServiceStub());
#endif

  DCHECK(create_mode == CREATE_MODE_ASYNCHRONOUS ||
         create_mode == CREATE_MODE_SYNCHRONOUS);
  bool async_prefs = create_mode == CREATE_MODE_ASYNCHRONOUS;

  chrome::RegisterUserPrefs(pref_registry_);

  {
    // On startup, preference loading is always synchronous so a scoped timer
    // will work here.
    startup_metric_utils::ScopedSlowStartupUMA
        scoped_timer("Startup.SlowStartupPreferenceLoading");
    prefs_.reset(chrome_prefs::CreateProfilePrefs(
        GetPrefFilePath(),
        sequenced_task_runner,
        policy_service_.get(),
        new ExtensionPrefStore(
            ExtensionPrefValueMapFactory::GetForProfile(this), false),
        pref_registry_,
        async_prefs));
    // Register on BrowserContext.
    components::UserPrefs::Set(this, prefs_.get());
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

void ProfileImpl::DoFinalInit(bool is_new_profile) {
  PrefService* prefs = GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kGoogleServicesUsername,
      base::Bind(&ProfileImpl::UpdateProfileUserNameCache,
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

  // It would be nice to use PathService for fetching this directory, but
  // the cache directory depends on the profile directory, which isn't available
  // to PathService.
  chrome::GetUserCacheDirectory(path_, &base_cache_path_);
  // Always create the cache directory asynchronously.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(base_cache_path_,
                                          BrowserThread::GetBlockingPool());
  CreateProfileDirectory(sequenced_task_runner, base_cache_path_);

  // Now that the profile is hooked up to receive pref change notifications to
  // kGoogleServicesUsername, initialize components that depend on it to reflect
  // the current value.
  UpdateProfileUserNameCache();
  GAIAInfoUpdateServiceFactory::GetForProfile(this);

  PrefService* local_state = g_browser_process->local_state();
  ssl_config_service_manager_.reset(
      SSLConfigServiceManager::CreateDefaultManager(local_state, prefs));

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
  base::FilePath server_bound_cert_path = GetPath();
  server_bound_cert_path =
      server_bound_cert_path.Append(chrome::kOBCertFilename);
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
  bool restore_old_session_cookies =
      (GetLastSessionExitType() == Profile::EXIT_CRASHED ||
       startup_pref_type == SessionStartupPref::LAST);

  InitHostZoomMap();

  // Make sure we initialize the ProfileIOData after everything else has been
  // initialized that we might be reading from the IO thread.

  io_data_.Init(cookie_path, server_bound_cert_path, cache_path,
                cache_max_size, media_cache_path, media_cache_max_size,
                extensions_cookie_path, GetPath(), infinite_cache_path,
                predictor_,
                restore_old_session_cookies,
                GetSpecialStoragePolicy());

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      PluginPrefs::GetForProfile(this),
      io_data_.GetResourceContextNoInit());
#endif

  // Delay README creation to not impact startup performance.
  BrowserThread::PostDelayedTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&EnsureReadmeFile, GetPath()),
        base::TimeDelta::FromMilliseconds(create_readme_delay_ms));

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRestoreSessionState)) {
    content::BrowserContext::GetDefaultStoragePartition(this)->
        GetDOMStorageContext()->SetSaveSessionStorageOnDisk();
  }

  // Creation has been finished.
  if (delegate_)
    delegate_->OnProfileCreated(this, true, is_new_profile);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(this),
      content::NotificationService::NoDetails());

#if !defined(OS_CHROMEOS)
  // Listen for bookmark model load, to bootstrap the sync service.
  // On CrOS sync service will be initialized after sign in.
  if (!g_browser_process->profile_manager()->will_import()) {
    // If |will_import()| is true we add the observer in
    // ProfileManager::OnImportFinished().
    BookmarkModel* model = BookmarkModelFactory::GetForProfile(this);
    model->AddObserver(new BookmarkModelLoadedObserver(this));
  }
#endif

}

void ProfileImpl::InitHostZoomMap() {
  HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);
  host_zoom_map->SetDefaultZoomLevel(
      prefs_->GetDouble(prefs::kDefaultZoomLevel));

  const DictionaryValue* host_zoom_dictionary =
      prefs_->GetDictionary(prefs::kPerHostZoomLevels);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (host_zoom_dictionary != NULL) {
    for (DictionaryValue::Iterator i(*host_zoom_dictionary); !i.IsAtEnd();
         i.Advance()) {
      const std::string& host(i.key());
      double zoom_level = 0;

      bool success = i.value().GetAsDouble(&zoom_level);
      DCHECK(success);
      host_zoom_map->SetZoomLevelForHost(host, zoom_level);
    }
  }

  host_zoom_map->AddZoomLevelChangedCallback(zoom_callback_);
}

base::FilePath ProfileImpl::last_selected_directory() {
  return GetPrefs()->GetFilePath(prefs::kSelectFileLastDirectory);
}

void ProfileImpl::set_last_selected_directory(const base::FilePath& path) {
  GetPrefs()->SetFilePath(prefs::kSelectFileLastDirectory, path);
}

ProfileImpl::~ProfileImpl() {
  MaybeSendDestroyedNotification();

  HostZoomMap::GetForBrowserContext(this)->RemoveZoomLevelChangedCallback(
      zoom_callback_);

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
  if (off_the_record_profile_.get()) {
    ProfileDestroyer::DestroyOffTheRecordProfileNow(
        off_the_record_profile_.get());
  } else {
    ExtensionPrefValueMapFactory::GetForProfile(this)->
        ClearAllIncognitoSessionOnlyPreferences();
  }

  ProfileDependencyManager::GetInstance()->DestroyProfileServices(this);

  if (top_sites_.get())
    top_sites_->Shutdown();

  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();

  if (host_content_settings_map_)
    host_content_settings_map_->ShutdownOnUIThread();

#if defined(ENABLE_CONFIGURATION_POLICY) && defined(ENABLE_MANAGED_USERS)
  if (managed_mode_policy_provider_)
    managed_mode_policy_provider_->Shutdown();
#endif

  // This causes the Preferences file to be written to disk.
  if (prefs_loaded)
    SetExitType(EXIT_NORMAL);
}

std::string ProfileImpl::GetProfileName() {
  return GetPrefs()->GetString(prefs::kGoogleServicesUsername);
}

base::FilePath ProfileImpl::GetPath() {
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
  if (!off_the_record_profile_.get()) {
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
  ExtensionPrefValueMapFactory::GetForProfile(this)->
      ClearAllIncognitoSessionOnlyPreferences();
}

bool ProfileImpl::HasOffTheRecordProfile() {
  return off_the_record_profile_.get() != NULL;
}

Profile* ProfileImpl::GetOriginalProfile() {
  return this;
}

ExtensionService* ProfileImpl::GetExtensionService() {
  return extensions::ExtensionSystem::Get(this)->extension_service();
}

ExtensionSpecialStoragePolicy*
    ProfileImpl::GetExtensionSpecialStoragePolicy() {
  if (!extension_special_storage_policy_.get()) {
    extension_special_storage_policy_ = new ExtensionSpecialStoragePolicy(
        CookieSettings::Factory::GetForProfile(this));
  }
  return extension_special_storage_policy_.get();
}

void ProfileImpl::OnPrefsLoaded(bool success) {
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

  ProfileDependencyManager::GetInstance()->CreateProfileServices(this, false);

  DCHECK(!net_pref_observer_.get());
  net_pref_observer_.reset(new NetPrefObserver(
      prefs_.get(),
      prerender::PrerenderManagerFactory::GetForProfile(this),
      predictor_));

  bool is_new_profile = prefs_->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_CREATED_NEW_PROFILE;
  ChromeVersionService::OnProfileLoaded(prefs_.get(), is_new_profile);
  DoFinalInit(is_new_profile);
}

bool ProfileImpl::WasCreatedByVersionOrLater(const std::string& version) {
  Version profile_version(ChromeVersionService::GetVersion(prefs_.get()));
  Version arg_version(version);
  return (profile_version.CompareTo(arg_version) >= 0);
}

void ProfileImpl::SetExitType(ExitType exit_type) {
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
    // ChromeFrame::EndSession().
    prefs_->CommitPendingWrite();
  }
}

Profile::ExitType ProfileImpl::GetLastSessionExitType() {
  // last_session_exited_cleanly_ is set when the preferences are loaded. Force
  // it to be set by asking for the prefs.
  GetPrefs();
  return last_session_exit_type_;
}

policy::ManagedModePolicyProvider* ProfileImpl::GetManagedModePolicyProvider() {
#if defined(ENABLE_CONFIGURATION_POLICY) && defined(ENABLE_MANAGED_USERS)
  return managed_mode_policy_provider_.get();
#else
  return NULL;
#endif
}

policy::PolicyService* ProfileImpl::GetPolicyService() {
  DCHECK(policy_service_.get());  // Should explicitly be initialized.
  return policy_service_.get();
}

PrefService* ProfileImpl::GetPrefs() {
  DCHECK(prefs_.get());  // Should explicitly be initialized.
  return prefs_.get();
}

PrefService* ProfileImpl::GetOffTheRecordPrefs() {
  DCHECK(prefs_.get());
  if (!otr_prefs_.get()) {
    // The new ExtensionPrefStore is ref_counted and the new PrefService
    // stores a reference so that we do not leak memory here.
    otr_prefs_.reset(prefs_->CreateIncognitoPrefService(
        new ExtensionPrefStore(
            ExtensionPrefValueMapFactory::GetForProfile(this), true)));
  }
  return otr_prefs_.get();
}

base::FilePath ProfileImpl::GetPrefFilePath() {
  base::FilePath pref_file_path = path_;
  pref_file_path = pref_file_path.Append(chrome::kPreferencesFilename);
  return pref_file_path;
}

net::URLRequestContextGetter* ProfileImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers) {
  return io_data_.CreateMainRequestContextGetter(
      protocol_handlers,
      g_browser_process->local_state(),
      g_browser_process->io_thread());
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
  return io_data_.GetMediaRequestContextGetter();
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
  return io_data_.GetIsolatedMediaRequestContextGetter(partition_path,
                                                       in_memory);
}

content::ResourceContext* ProfileImpl::GetResourceContext() {
  return io_data_.GetResourceContext();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContextForExtensions() {
  return io_data_.GetExtensionsRequestContextGetter();
}

net::URLRequestContextGetter*
ProfileImpl::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers) {
  return io_data_.CreateIsolatedAppRequestContextGetter(
      partition_path, in_memory, protocol_handlers);
}

net::SSLConfigService* ProfileImpl::GetSSLConfigService() {
  // If ssl_config_service_manager_ is null, this typically means that some
  // ProfileKeyedService is trying to create a RequestContext at startup, but
  // SSLConfigServiceManager is not initialized until DoFinalInit() which is
  // invoked after all ProfileKeyedServices have been initialized (see
  // http://crbug.com/171406).
  DCHECK(ssl_config_service_manager_.get()) <<
      "SSLConfigServiceManager is not initialized yet";
  return ssl_config_service_manager_->Get();
}

HostContentSettingsMap* ProfileImpl::GetHostContentSettingsMap() {
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(GetPrefs(), false);
  }
  return host_content_settings_map_.get();
}

content::GeolocationPermissionContext*
    ProfileImpl::GetGeolocationPermissionContext() {
  return ChromeGeolocationPermissionContextFactory::GetForProfile(this);
}

content::SpeechRecognitionPreferences*
ProfileImpl::GetSpeechRecognitionPreferences() {
#if defined(ENABLE_INPUT_SPEECH)
  return ChromeSpeechRecognitionPreferences::GetForProfile(this);
#else
  return NULL;
#endif
}

DownloadManagerDelegate* ProfileImpl::GetDownloadManagerDelegate() {
  return DownloadServiceFactory::GetForProfile(this)->
      GetDownloadManagerDelegate();
}

quota::SpecialStoragePolicy* ProfileImpl::GetSpecialStoragePolicy() {
  return GetExtensionSpecialStoragePolicy();
}

ProtocolHandlerRegistry* ProfileImpl::GetProtocolHandlerRegistry() {
  // TODO(smckay): Update all existing callers to use
  // ProtocolHandlerRegistryFactory. Once that's done, this method
  // can be nuked from Profile and ProfileImpl.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return ProtocolHandlerRegistryFactory::GetForProfile(this);
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
    top_sites_ = new history::TopSites(this);
    top_sites_->Init(GetPath().Append(chrome::kTopSitesFilename));
  }
  return top_sites_;
}

history::TopSites* ProfileImpl::GetTopSitesWithoutCreating() {
  return top_sites_;
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
  DictionaryValue* host_zoom_dictionary = update.Get();
  if (level == host_zoom_map->GetDefaultZoomLevel()) {
    host_zoom_dictionary->RemoveWithoutPathExpansion(change.host, NULL);
  } else {
    host_zoom_dictionary->SetWithoutPathExpansion(
        change.host, Value::CreateDoubleValue(level));
  }
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
    case APP_LOCALE_CHANGED_VIA_LOGIN: {
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
        if (!backup_locale.empty())
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
  local_state->SetString(prefs::kApplicationLocale, new_locale);

  if (chromeos::UserManager::Get()->IsCurrentUserOwner())
    local_state->SetString(prefs::kOwnerLocale, new_locale);
}

void ProfileImpl::OnLogin() {
  if (locale_change_guard_ == NULL)
    locale_change_guard_.reset(new chromeos::LocaleChangeGuard(this));
  locale_change_guard_->OnLogin();
}

void ProfileImpl::SetupChromeOSEnterpriseExtensionObserver() {
  DCHECK(!chromeos_enterprise_extension_observer_.get());
  chromeos_enterprise_extension_observer_.reset(
      new chromeos::EnterpriseExtensionObserver(this));
}

void ProfileImpl::InitChromeOSPreferences() {
  chromeos_preferences_.reset(new chromeos::Preferences());
  chromeos_preferences_->Init(PrefServiceSyncable::FromProfile(this));
}
#endif  // defined(OS_CHROMEOS)

PrefProxyConfigTracker* ProfileImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_.get()) {
    pref_proxy_config_tracker_.reset(
        ProxyServiceFactory::CreatePrefProxyConfigTracker(GetPrefs()));
  }
  return pref_proxy_config_tracker_.get();
}

chrome_browser_net::Predictor* ProfileImpl::GetNetworkPredictor() {
  return predictor_;
}

void ProfileImpl::ClearNetworkingHistorySince(base::Time time,
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
    GURL home_page(URLFixerUpper::FixupRelativeFile(browser_directory,
        command_line.GetSwitchValuePath(switches::kHomePage)));
    if (home_page.is_valid())
      return home_page;
  }

  if (GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
    return GURL(chrome::kChromeUINewTabURL);
  GURL home_page(URLFixerUpper::FixupURL(
      GetPrefs()->GetString(prefs::kHomePage),
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
    cache.SetUserNameOfProfileAtIndex(index, UTF8ToUTF16(user_name));
  }
}

void ProfileImpl::UpdateProfileNameCache() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(GetPath());
  if (index != std::string::npos) {
    std::string profile_name =
        GetPrefs()->GetString(prefs::kProfileName);
    cache.SetNameOfProfileAtIndex(index, UTF8ToUTF16(profile_name));
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
