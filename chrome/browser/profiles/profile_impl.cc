// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_navigation_observer.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_settings.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/speech/chrome_speech_input_manager.h"
#include "chrome/browser/spellchecker/spellcheck_profile.h"
#include "chrome/browser/sync/profile_sync_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/transport_security_persister.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/extension_icon_source.h"
#include "chrome/browser/user_style_sheet_watcher.h"
#include "chrome/browser/visitedlink/visitedlink_event_listener.h"
#include "chrome/browser/visitedlink/visitedlink_master.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/browser_thread.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/speech/speech_input_manager.h"
#include "content/browser/ssl/ssl_host_state.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/locale_settings.h"
#include "net/base/transport_security_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/database/database_tracker.h"
#include "webkit/quota/quota_manager.h"

#if defined(OS_WIN)
#include "chrome/browser/instant/promo_counter.h"
#include "chrome/browser/password_manager/password_store_win.h"
#include "chrome/installer/util/install_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/keychain_mac.h"
#include "chrome/browser/mock_keychain_mac.h"
#include "chrome/browser/password_manager/password_store_mac.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/enterprise_extension_observer.h"
#elif defined(OS_POSIX) && !defined(OS_CHROMEOS)
#include "base/nix/xdg_util.h"
#if defined(USE_GNOME_KEYRING)
#include "chrome/browser/password_manager/native_backend_gnome_x.h"
#endif
#include "chrome/browser/password_manager/native_backend_kwallet_x.h"
#include "chrome/browser/password_manager/password_store_x.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/locale_change_guard.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/prerender_condition_network.h"
#endif

using base::Time;
using base::TimeDelta;

namespace {

// Delay, in milliseconds, before we explicitly create the SessionService.
static const int kCreateSessionServiceDelayMS = 500;

#if defined(OS_MACOSX)
// Capacity for mock keychain used for testing.
static const int kMockKeychainSize = 1000;
#endif

enum ContextType {
  kNormalContext,
  kMediaContext
};

// Gets the cache parameters from the command line. |type| is the type of
// request context that we need, |cache_path| will be set to the user provided
// path, or will not be touched if there is not an argument. |max_size| will
// be the user provided value or zero by default.
void GetCacheParameters(ContextType type, FilePath* cache_path,
                        int* max_size) {
  DCHECK(cache_path);
  DCHECK(max_size);

  // Override the cache location if specified by the user.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDiskCacheDir)) {
    *cache_path = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        switches::kDiskCacheDir);
  }
#if !defined(OS_CHROMEOS)
  // And if a policy is set this should have even higher precedence.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs && prefs->IsManagedPreference(prefs::kDiskCacheDir))
    *cache_path = prefs->GetFilePath(prefs::kDiskCacheDir);
#endif

  const char* arg = kNormalContext == type ? switches::kDiskCacheSize :
                                             switches::kMediaCacheSize;
  std::string value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(arg);

  // By default we let the cache determine the right size.
  *max_size = 0;
  if (!base::StringToInt(value, max_size)) {
    *max_size = 0;
  } else if (*max_size < 0) {
    *max_size = 0;
  }
}

FilePath GetCachePath(const FilePath& base) {
  return base.Append(chrome::kCacheDirname);
}

FilePath GetMediaCachePath(const FilePath& base) {
  return base.Append(chrome::kMediaCacheDirname);
}

// Simple task to log the size of the current profile.
class ProfileSizeTask : public Task {
 public:
  explicit ProfileSizeTask(const FilePath& path) : path_(path) {}
  virtual ~ProfileSizeTask() {}

  virtual void Run();
 private:
  FilePath path_;
};

void ProfileSizeTask::Run() {
  int64 size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = file_util::ComputeFilesSize(path_, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size  / (1024 * 1024));
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);
}

}  // namespace

// static
Profile* Profile::CreateProfile(const FilePath& path) {
  if (!file_util::PathExists(path)) {
    // TODO(tc): http://b/1094718 Bad things happen if we can't write to the
    // profile directory.  We should eventually be able to run in this
    // situation.
    if (!file_util::CreateDirectory(path))
      return NULL;
  }
  return new ProfileImpl(path, NULL);
}

// static
Profile* Profile::CreateProfileAsync(const FilePath& path,
                                     Profile::Delegate* delegate) {
  DCHECK(delegate);
  // This is safe while all file operations are done on the FILE thread.
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          NewRunnableFunction(&file_util::CreateDirectory,
                                              path));
  // Async version.
  return new ProfileImpl(path, delegate);
}

// static
void ProfileImpl::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kClearSiteDataOnExit,
                             false,
                             PrefService::SYNCABLE_PREF);
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
  prefs->RegisterIntegerPref(prefs::kLocalProfileId,
                             kInvalidLocalProfileId,
                             PrefService::UNSYNCABLE_PREF);
  // Notice that the preprocessor conditions above are exactly those that will
  // result in using PasswordStoreX in CreatePasswordStore() below.
  PasswordStoreX::RegisterUserPrefs(prefs);
#endif
}

ProfileImpl::ProfileImpl(const FilePath& path,
                         Profile::Delegate* delegate)
    : path_(path),
      visited_link_event_listener_(new VisitedLinkEventListener()),
      extension_devtools_manager_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_data_(this)),
      host_content_settings_map_(NULL),
      host_zoom_map_(NULL),
      history_service_created_(false),
      favicon_service_created_(false),
      created_web_data_service_(false),
      created_password_store_(false),
      created_download_manager_(false),
      start_time_(Time::Now()),
#if defined(OS_WIN)
      checked_instant_promo_(false),
#endif
      delegate_(delegate),
      predictor_(NULL) {
  DCHECK(!path.empty()) << "Using an empty path will attempt to write " <<
                            "profile files to the root directory!";

  create_session_service_timer_.Start(FROM_HERE,
      TimeDelta::FromMilliseconds(kCreateSessionServiceDelayMS), this,
      &ProfileImpl::EnsureSessionServiceCreated);

  // Determine if prefetch is enabled for this profile.
  // If not profile_manager is present, it means we are in a unittest.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  predictor_ = chrome_browser_net::Predictor::CreatePredictor(
      !command_line->HasSwitch(switches::kDisablePreconnect),
      g_browser_process->profile_manager() == NULL);

  if (delegate_) {
    prefs_.reset(PrefService::CreatePrefService(
        GetPrefFilePath(),
        new ExtensionPrefStore(GetExtensionPrefValueMap(), false),
        true));
    // Wait for the notifcation that prefs has been loaded (successfully or
    // not).
    registrar_.Add(this, chrome::NOTIFICATION_PREF_INITIALIZATION_COMPLETED,
                   Source<PrefService>(prefs_.get()));
  } else {
    // Load prefs synchronously.
    prefs_.reset(PrefService::CreatePrefService(
        GetPrefFilePath(),
        new ExtensionPrefStore(GetExtensionPrefValueMap(), false),
        false));
    OnPrefsLoaded(true);
  }
}

void ProfileImpl::DoFinalInit() {
  PrefService* prefs = GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kSpellCheckDictionary, this);
  pref_change_registrar_.Add(prefs::kEnableSpellCheck, this);
  pref_change_registrar_.Add(prefs::kEnableAutoSpellCorrect, this);
  pref_change_registrar_.Add(prefs::kSpeechInputCensorResults, this);
  pref_change_registrar_.Add(prefs::kClearSiteDataOnExit, this);
  pref_change_registrar_.Add(prefs::kGoogleServicesUsername, this);
  pref_change_registrar_.Add(prefs::kDefaultZoomLevel, this);

  // It would be nice to use PathService for fetching this directory, but
  // the cache directory depends on the profile directory, which isn't available
  // to PathService.
  chrome::GetUserCacheDirectory(path_, &base_cache_path_);
  if (!delegate_) {
    if (!file_util::CreateDirectory(base_cache_path_))
      LOG(FATAL) << "Failed to create " << base_cache_path_.value();
  } else {
    // Async profile loading is used, so call this on the FILE thread instead.
    // It is safe since all other file operations should also be done there.
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            NewRunnableFunction(&file_util::CreateDirectory,
                                                base_cache_path_));
  }

#if !defined(OS_CHROMEOS)
  // Listen for bookmark model load, to bootstrap the sync service.
  // On CrOS sync service will be initialized after sign in.
  registrar_.Add(this, chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
                 Source<Profile>(this));
#endif

  PrefService* local_state = g_browser_process->local_state();
  ssl_config_service_manager_.reset(
      SSLConfigServiceManager::CreateDefaultManager(local_state));

  PinnedTabServiceFactory::InitForProfile(this);

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

  extension_info_map_ = new ExtensionInfoMap();

  InitRegisteredProtocolHandlers();

  clear_local_state_on_exit_ = prefs->GetBoolean(prefs::kClearSiteDataOnExit);
  if (clear_local_state_on_exit_) {
    UserMetrics::RecordAction(
        UserMetricsAction("ClearSiteDataOnExitEnabled"));
  } else {
    UserMetrics::RecordAction(
        UserMetricsAction("ClearSiteDataOnExitDisabled"));
  }

  // Log the profile size after a reasonable startup delay.
  BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE,
                                 new ProfileSizeTask(path_), 112000);

  InstantController::RecordMetrics(this);

  // Instantiates Metrics object for spellchecking for use.
  if (g_browser_process->metrics_service() &&
      g_browser_process->metrics_service()->recording_active())
    GetSpellCheckProfile()->StartRecordingMetrics(
        GetPrefs()->GetBoolean(prefs::kEnableSpellCheck));

  speech_input::ChromeSpeechInputManager::GetInstance()->set_censor_results(
      prefs->GetBoolean(prefs::kSpeechInputCensorResults));

  FilePath cookie_path = GetPath();
  cookie_path = cookie_path.Append(chrome::kCookieFilename);
  FilePath origin_bound_cert_path = GetPath();
  origin_bound_cert_path =
      origin_bound_cert_path.Append(chrome::kOBCertFilename);
  FilePath cache_path = base_cache_path_;
  int cache_max_size;
  GetCacheParameters(kNormalContext, &cache_path, &cache_max_size);
  cache_path = GetCachePath(cache_path);

  FilePath media_cache_path = base_cache_path_;
  int media_cache_max_size;
  GetCacheParameters(kMediaContext, &media_cache_path, &media_cache_max_size);
  media_cache_path = GetMediaCachePath(media_cache_path);

  FilePath extensions_cookie_path = GetPath();
  extensions_cookie_path =
      extensions_cookie_path.Append(chrome::kExtensionsCookieFilename);

  FilePath app_path = GetPath().Append(chrome::kIsolatedAppStateDirname);

  // Make sure we initialize the ProfileIOData after everything else has been
  // initialized that we might be reading from the IO thread.

  io_data_.Init(cookie_path, origin_bound_cert_path, cache_path,
                cache_max_size, media_cache_path, media_cache_max_size,
                extensions_cookie_path, app_path, predictor_,
                g_browser_process->local_state(),
                g_browser_process->io_thread());

  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      PluginPrefs::GetForProfile(this), &GetResourceContext());

  // Creation has been finished.
  if (delegate_)
    delegate_->OnProfileCreated(this, true);

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      Source<Profile>(this),
      NotificationService::NoDetails());
}

void ProfileImpl::InitExtensions(bool extensions_enabled) {
  if (user_script_master_ || extension_service_.get())
    return;  // Already initialized.

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
      switches::kEnableExtensionTimelineApi)) {
    extension_devtools_manager_ = new ExtensionDevToolsManager(this);
  }

  extension_process_manager_.reset(ExtensionProcessManager::Create(this));
  extension_event_router_.reset(new ExtensionEventRouter(this));
  extension_message_service_ = new ExtensionMessageService(this);
  extension_navigation_observer_.reset(new ExtensionNavigationObserver(this));

  ExtensionErrorReporter::Init(true);  // allow noisy errors.

  user_script_master_ = new UserScriptMaster(this);

  bool autoupdate_enabled = true;
#if defined(OS_CHROMEOS)
  if (!extensions_enabled)
    autoupdate_enabled = false;
  else
    autoupdate_enabled = !command_line->HasSwitch(switches::kGuestSession);
#endif
  extension_service_.reset(new ExtensionService(
      this,
      CommandLine::ForCurrentProcess(),
      GetPath().AppendASCII(ExtensionService::kInstallDirectoryName),
      extension_prefs_.get(),
      extension_settings_.get(),
      autoupdate_enabled,
      extensions_enabled));

  RegisterComponentExtensions();

#if defined(GOOGLE_CHROME_BUILD)
  // If first run and brand code not equal to ECDB, install default apps.
#if defined(OS_WIN)
  string16 brand;
  GoogleUpdateSettings::GetBrand(&brand);
#elif defined(OS_MACOSX)
  std::string brand = keystone_glue::BrandCode();
#else
  std::string brand;
#endif
  // TODO(caitkp): when we move to multi-profiles (M16) we will want to change
  // this check, as |FirstRun::IsChromeFirstRun()| checks for the first run
  // ever, not first run per profile.
  if (FirstRun::IsChromeFirstRun() &&
      !LowerCaseEqualsASCII(brand, "ecdb")) {
    InstallDefaultApps();
  }
#endif

  extension_service_->Init();

  if (extensions_enabled) {
    // Load any extensions specified with --load-extension.
    if (command_line->HasSwitch(switches::kLoadExtension)) {
      FilePath path = command_line->GetSwitchValuePath(
          switches::kLoadExtension);
      extension_service_->LoadExtensionFromCommandLine(path);
    }
  }

  // Make the chrome://extension-icon/ resource available.
  GetChromeURLDataManager()->AddDataSource(new ExtensionIconSource(this));

  // Initialize extension event routers. Note that on Chrome OS, this will
  // not succeed if the user has not logged in yet, in which case the
  // event routers are initialized in LoginUtilsImpl::CompleteLogin instead.
  // The InitEventRouters call used to be in BrowserMain, because when bookmark
  // import happened on first run, the bookmark bar was not being correctly
  // initialized (see issue 40144). Now that bookmarks aren't imported and
  // the event routers need to be initialized for every profile individually,
  // initialize them with the extension service.
  // If this profile is being created as part of the import process, never
  // initialize the event routers. If import is going to run in a separate
  // process (the profile itself is on the main process), wait for import to
  // finish before initializing the routers.
  if (!command_line->HasSwitch(switches::kImport) &&
      !command_line->HasSwitch(switches::kImportFromFile)) {
    if (g_browser_process->profile_manager()->will_import()) {
      extension_service_->InitEventRoutersAfterImport();
    } else {
      extension_service_->InitEventRouters();
    }
  }
}

void ProfileImpl::InstallDefaultApps() {
  FilePath apps_dir;
  FilePath file;
  std::list<FilePath> crx_path_list;

  if (PathService::Get(chrome::DIR_DEFAULT_APPS, &apps_dir)) {
    file_util::FileEnumerator file_enumerator(apps_dir, false,
        file_util::FileEnumerator::FILES);
    while (!(file = file_enumerator.Next()).value().empty()) {
      if (LowerCaseEqualsASCII(file.Extension(), ".crx"))
        crx_path_list.push_back(file);
    }
  }

  for (std::list<FilePath>::iterator iter = crx_path_list.begin();
       iter != crx_path_list.end(); ++iter) {
    scoped_refptr<CrxInstaller> crx_installer =
        extension_service_->MakeCrxInstaller(NULL);
    crx_installer->set_allow_silent_install(true);
    crx_installer->set_delete_source(false);
    crx_installer->set_install_cause(extension_misc::INSTALL_CAUSE_UPDATE);
    crx_installer->InstallCrx(*iter);
  }
}

void ProfileImpl::RegisterComponentExtensions() {
  // Register the component extensions.
  //
  // Component extension manifest must contain a 'key' property with a unique
  // public key, serialized in base64. You can create a suitable value with the
  // following commands on a unixy system:
  //
  //   ssh-keygen -t rsa -b 1024 -N '' -f /tmp/key.pem
  //   openssl rsa -pubout -outform DER < /tmp/key.pem 2>/dev/null | base64 -w 0
  typedef std::list<std::pair<FilePath::StringType, int> >
      ComponentExtensionList;
  ComponentExtensionList component_extensions;

  // Bookmark manager.
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("bookmark_manager"),
      IDR_BOOKMARKS_MANIFEST));

#if defined(FILE_MANAGER_EXTENSION)
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
#ifndef NDEBUG
  if (command_line->HasSwitch(switches::kFileManagerExtensionPath)) {
    FilePath filemgr_extension_path =
        command_line->GetSwitchValuePath(switches::kFileManagerExtensionPath);
    component_extensions.push_back(std::make_pair(
        filemgr_extension_path.value(),
        IDR_FILEMANAGER_MANIFEST));
  } else {
    component_extensions.push_back(std::make_pair(
        FILE_PATH_LITERAL("file_manager"),
        IDR_FILEMANAGER_MANIFEST));
  }
#else
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("file_manager"),
      IDR_FILEMANAGER_MANIFEST));
#endif
#endif

#if defined(TOUCH_UI)
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("keyboard"),
      IDR_KEYBOARD_MANIFEST));
#endif

#if defined(OS_CHROMEOS)
    component_extensions.push_back(std::make_pair(
        FILE_PATH_LITERAL("/usr/share/chromeos-assets/mobile"),
        IDR_MOBILE_MANIFEST));

    if (command_line->HasSwitch(switches::kAuthExtensionPath)) {
      FilePath auth_extension_path =
          command_line->GetSwitchValuePath(switches::kAuthExtensionPath);
      component_extensions.push_back(std::make_pair(
          auth_extension_path.value(),
          IDR_GAIA_TEST_AUTH_MANIFEST));
    } else {
      component_extensions.push_back(std::make_pair(
          FILE_PATH_LITERAL("/usr/share/chromeos-assets/gaia_auth"),
          IDR_GAIA_AUTH_MANIFEST));
    }

#if defined(OFFICIAL_BUILD)
    if (browser_defaults::enable_help_app) {
      component_extensions.push_back(std::make_pair(
          FILE_PATH_LITERAL("/usr/share/chromeos-assets/helpapp"),
          IDR_HELP_MANIFEST));
    }
#endif
#endif

  // Web Store.
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("web_store"),
      IDR_WEBSTORE_MANIFEST));

#if !defined(OS_CHROMEOS)
  // Cloud Print component app. Not required on Chrome OS.
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("cloud_print"),
      IDR_CLOUDPRINT_MANIFEST));
#endif  // !defined(OS_CHROMEOS)

  for (ComponentExtensionList::iterator iter = component_extensions.begin();
    iter != component_extensions.end(); ++iter) {
    FilePath path(iter->first);
    if (!path.IsAbsolute()) {
      if (PathService::Get(chrome::DIR_RESOURCES, &path)) {
        path = path.Append(iter->first);
      } else {
        NOTREACHED();
      }
    }

    std::string manifest =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            iter->second).as_string();
    extension_service_->register_component_extension(
        ExtensionService::ComponentExtensionInfo(manifest, path));
  }

#if defined(OS_CHROMEOS)
  // Register access extensions only if accessibility is enabled.
  if (g_browser_process->local_state()->
      GetBoolean(prefs::kAccessibilityEnabled)) {
    FilePath path = FilePath(extension_misc::kAccessExtensionPath)
        .AppendASCII(extension_misc::kChromeVoxDirectoryName);
    std::string manifest =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_CHROMEVOX_MANIFEST).as_string();
    extension_service_->register_component_extension(
        ExtensionService::ComponentExtensionInfo(manifest, path));
  }
#endif
}

void ProfileImpl::InitPromoResources() {
  if (promo_resource_service_)
    return;

  promo_resource_service_ = new PromoResourceService(this);
  promo_resource_service_->StartAfterDelay();
}

void ProfileImpl::InitRegisteredProtocolHandlers() {
  if (protocol_handler_registry_)
    return;
  protocol_handler_registry_ = new ProtocolHandlerRegistry(this,
      new ProtocolHandlerRegistry::Delegate());
  protocol_handler_registry_->Load();
}

FilePath ProfileImpl::last_selected_directory() {
  return GetPrefs()->GetFilePath(prefs::kSelectFileLastDirectory);
}

void ProfileImpl::set_last_selected_directory(const FilePath& path) {
  GetPrefs()->SetFilePath(prefs::kSelectFileLastDirectory, path);
}

ProfileImpl::~ProfileImpl() {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_DESTROYED,
      Source<Profile>(this),
      NotificationService::NoDetails());

  // There shouldn't be any browser window associated with this profile or an
  // incognito profile based on this profile.
  CHECK(!BrowserList::FindAnyBrowser(this, false /* match_incognito */));

  if (appcache_service_ && clear_local_state_on_exit_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(
            appcache_service_.get(),
            &appcache::AppCacheService::set_clear_local_state_on_exit,
            true));
  }

  if (webkit_context_.get())
    webkit_context_->DeleteSessionOnlyData();

  StopCreateSessionServiceTimer();

  // Remove pref observers
  pref_change_registrar_.RemoveAll();

  // The sync service needs to be deleted before the services it calls.
  // TODO(stevet): Make ProfileSyncService into a PKS and let the PDM take care
  // of the cleanup below.
  sync_service_.reset();

  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
      &GetResourceContext());

  ProfileDependencyManager::GetInstance()->DestroyProfileServices(this);

  if (db_tracker_) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(
            db_tracker_.get(),
            &webkit_database::DatabaseTracker::Shutdown));
  }

  // DownloadManager is lazily created, so check before accessing it.
  if (download_manager_.get()) {
    // The download manager queries the history system and should be shut down
    // before the history is shut down so it can properly cancel all requests.
    download_manager_->Shutdown();
    download_manager_ = NULL;
  }

  // Password store uses WebDB, shut it down before the WebDB has been shutdown.
  if (password_store_.get())
    password_store_->Shutdown();

  // Both HistoryService and WebDataService maintain threads for background
  // processing. Its possible each thread still has tasks on it that have
  // increased the ref count of the service. In such a situation, when we
  // decrement the refcount, it won't be 0, and the threads/databases aren't
  // properly shut down. By explicitly calling Cleanup/Shutdown we ensure the
  // databases are properly closed.
  if (web_data_service_.get())
    web_data_service_->Shutdown();

  if (top_sites_.get())
    top_sites_->Shutdown();

  if (bookmark_bar_model_.get()) {
    // It's possible that bookmarks haven't loaded and history is waiting for
    // bookmarks to complete loading. In such a situation history can't shutdown
    // (meaning if we invoked history_service_->Cleanup now, we would
    // deadlock). To break the deadlock we tell BookmarkModel it's about to be
    // deleted so that it can release the signal history is waiting on, allowing
    // history to shutdown (history_service_->Cleanup to complete). In such a
    // scenario history sees an incorrect view of bookmarks, but it's better
    // than a deadlock.
    bookmark_bar_model_->Cleanup();
  }

  if (history_service_.get())
    history_service_->Cleanup();

  if (io_data_.HasMainRequestContext() &&
      default_request_context_ == GetRequestContext()) {
    default_request_context_ = NULL;
  }

  // HistoryService may call into the BookmarkModel, as such we need to
  // delete HistoryService before the BookmarkModel. The destructor for
  // HistoryService will join with HistoryService's backend thread so that
  // by the time the destructor has finished we're sure it will no longer call
  // into the BookmarkModel.
  history_service_ = NULL;
  bookmark_bar_model_.reset();

  // FaviconService depends on HistoryServce so make sure we delete
  // HistoryService first.
  favicon_service_.reset();

  if (extension_message_service_)
    extension_message_service_->DestroyingProfile();

  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();

  if (protocol_handler_registry_)
    protocol_handler_registry_->Finalize();

  if (host_content_settings_map_)
    host_content_settings_map_->ShutdownOnUIThread();

  // This causes the Preferences file to be written to disk.
  MarkAsCleanShutdown();
}

std::string ProfileImpl::GetProfileName() {
  return GetPrefs()->GetString(prefs::kGoogleServicesUsername);
}

FilePath ProfileImpl::GetPath() {
  return path_;
}

bool ProfileImpl::IsOffTheRecord() {
  return false;
}

Profile* ProfileImpl::GetOffTheRecordProfile() {
  if (!off_the_record_profile_.get()) {
    scoped_ptr<Profile> p(CreateOffTheRecordProfile());
    off_the_record_profile_.swap(p);

    NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_CREATED,
        Source<Profile>(off_the_record_profile_.get()),
        NotificationService::NoDetails());
  }
  return off_the_record_profile_.get();
}

void ProfileImpl::DestroyOffTheRecordProfile() {
  off_the_record_profile_.reset();
  extension_pref_value_map_->ClearAllIncognitoSessionOnlyPreferences();
}

bool ProfileImpl::HasOffTheRecordProfile() {
  return off_the_record_profile_.get() != NULL;
}

Profile* ProfileImpl::GetOriginalProfile() {
  return this;
}

ChromeAppCacheService* ProfileImpl::GetAppCacheService() {
  CreateQuotaManagerAndClients();
  return appcache_service_;
}

webkit_database::DatabaseTracker* ProfileImpl::GetDatabaseTracker() {
  CreateQuotaManagerAndClients();
  return db_tracker_;
}

VisitedLinkMaster* ProfileImpl::GetVisitedLinkMaster() {
  if (!visited_link_master_.get()) {
    scoped_ptr<VisitedLinkMaster> visited_links(
      new VisitedLinkMaster(visited_link_event_listener_.get(), this));
    if (!visited_links->Init())
      return NULL;
    visited_link_master_.swap(visited_links);
  }

  return visited_link_master_.get();
}

ExtensionService* ProfileImpl::GetExtensionService() {
  return extension_service_.get();
}

UserScriptMaster* ProfileImpl::GetUserScriptMaster() {
  return user_script_master_.get();
}

ExtensionDevToolsManager* ProfileImpl::GetExtensionDevToolsManager() {
  return extension_devtools_manager_.get();
}

ExtensionProcessManager* ProfileImpl::GetExtensionProcessManager() {
  return extension_process_manager_.get();
}

ExtensionMessageService* ProfileImpl::GetExtensionMessageService() {
  return extension_message_service_.get();
}

ExtensionEventRouter* ProfileImpl::GetExtensionEventRouter() {
  return extension_event_router_.get();
}

ExtensionSpecialStoragePolicy*
    ProfileImpl::GetExtensionSpecialStoragePolicy() {
  if (!extension_special_storage_policy_.get()) {
    extension_special_storage_policy_ =
        new ExtensionSpecialStoragePolicy(GetHostContentSettingsMap());
  }
  return extension_special_storage_policy_.get();
}

SSLHostState* ProfileImpl::GetSSLHostState() {
  if (!ssl_host_state_.get())
    ssl_host_state_.reset(new SSLHostState());

  DCHECK(ssl_host_state_->CalledOnValidThread());
  return ssl_host_state_.get();
}

net::TransportSecurityState*
    ProfileImpl::GetTransportSecurityState() {
  if (!transport_security_state_.get()) {
    transport_security_state_ = new net::TransportSecurityState(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kHstsHosts));
    transport_security_persister_ =
        new TransportSecurityPersister(transport_security_state_.get(),
                                       path_,
                                       false /* read-write */);
    transport_security_persister_->Init();
  }

  return transport_security_state_.get();
}

void ProfileImpl::OnPrefsLoaded(bool success) {
  if (!success) {
    DCHECK(delegate_);
    delegate_->OnProfileCreated(this, false);
    return;
  }

  // The Profile class and ProfileManager class may read some prefs so
  // register known prefs as soon as possible.
  Profile::RegisterUserPrefs(prefs_.get());
  browser::RegisterUserPrefs(prefs_.get());
  // TODO(mirandac): remove migration code after 6 months (crbug.com/69995).
  if (g_browser_process->local_state()) {
    browser::MigrateBrowserPrefs(prefs_.get(),
                                 g_browser_process->local_state());
  }

  // The last session exited cleanly if there is no pref for
  // kSessionExitedCleanly or the value for kSessionExitedCleanly is true.
  last_session_exited_cleanly_ =
      prefs_->GetBoolean(prefs::kSessionExitedCleanly);
  // Mark the session as open.
  prefs_->SetBoolean(prefs::kSessionExitedCleanly, false);
  // Make sure we save to disk that the session has opened.
  prefs_->ScheduleSavePersistentPrefs();

  // Ensure that preferences set by extensions are restored in the profile
  // as early as possible. The constructor takes care of that.
  extension_prefs_.reset(new ExtensionPrefs(
      prefs_.get(),
      GetPath().AppendASCII(ExtensionService::kInstallDirectoryName),
      GetExtensionPrefValueMap()));

  extension_settings_ = new ExtensionSettings(
      GetPath().AppendASCII(ExtensionService::kSettingsDirectoryName));

  ProfileDependencyManager::GetInstance()->CreateProfileServices(this, false);

  DCHECK(!net_pref_observer_.get());
  net_pref_observer_.reset(
      new NetPrefObserver(prefs_.get(), GetPrerenderManager(), predictor_));

  DoFinalInit();
}

PrefService* ProfileImpl::GetPrefs() {
  DCHECK(prefs_.get());  // Should explicitly be initialized.
  return prefs_.get();
}

PrefService* ProfileImpl::GetOffTheRecordPrefs() {
  if (!otr_prefs_.get()) {
    // The new ExtensionPrefStore is ref_counted and the new PrefService
    // stores a reference so that we do not leak memory here.
    otr_prefs_.reset(GetPrefs()->CreateIncognitoPrefService(
        new ExtensionPrefStore(GetExtensionPrefValueMap(), true)));
  }
  return otr_prefs_.get();
}

FilePath ProfileImpl::GetPrefFilePath() {
  FilePath pref_file_path = path_;
  pref_file_path = pref_file_path.Append(chrome::kPreferencesFilename);
  return pref_file_path;
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContext() {
  net::URLRequestContextGetter* request_context =
      io_data_.GetMainRequestContextGetter();
  // The first request context is always a normal (non-OTR) request context.
  // Even when Chromium is started in OTR mode, a normal profile is always
  // created first.
  if (!default_request_context_)
    default_request_context_ = request_context;

  return request_context;
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  if (extension_service_.get()) {
    const Extension* installed_app = extension_service_->
        GetInstalledAppForRenderer(renderer_child_id);
    if (installed_app != NULL && installed_app->is_storage_isolated() &&
        installed_app->HasAPIPermission(
            ExtensionAPIPermission::kExperimental)) {
      return GetRequestContextForIsolatedApp(installed_app->id());
    }
  }
  return GetRequestContext();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContextForMedia() {
  return io_data_.GetMediaRequestContextGetter();
}

const content::ResourceContext& ProfileImpl::GetResourceContext() {
  return io_data_.GetResourceContext();
}

FaviconService* ProfileImpl::GetFaviconService(ServiceAccessType sat) {
  if (!favicon_service_created_) {
    favicon_service_created_ = true;
    favicon_service_.reset(new FaviconService(this));
  }
  return favicon_service_.get();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContextForExtensions() {
  return io_data_.GetExtensionsRequestContextGetter();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContextForIsolatedApp(
    const std::string& app_id) {
  return io_data_.GetIsolatedAppRequestContextGetter(app_id);
}

void ProfileImpl::RegisterExtensionWithRequestContexts(
    const Extension* extension) {
  base::Time install_time;
  if (extension->location() != Extension::COMPONENT) {
    install_time = GetExtensionService()->extension_prefs()->
        GetInstallTime(extension->id());
  }
  bool incognito_enabled =
      GetExtensionService()->IsIncognitoEnabled(extension->id());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(extension_info_map_.get(),
                        &ExtensionInfoMap::AddExtension,
                        make_scoped_refptr(extension),
                        install_time, incognito_enabled));
}

void ProfileImpl::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const extension_misc::UnloadedExtensionReason reason) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(extension_info_map_.get(),
                        &ExtensionInfoMap::RemoveExtension,
                        extension_id,
                        reason));
}

net::SSLConfigService* ProfileImpl::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

HostContentSettingsMap* ProfileImpl::GetHostContentSettingsMap() {
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(
        GetPrefs(), GetExtensionService(), false);
  }
  return host_content_settings_map_.get();
}

HostZoomMap* ProfileImpl::GetHostZoomMap() {
  if (!host_zoom_map_) {
    host_zoom_map_ = new HostZoomMap();
    host_zoom_map_->set_default_zoom_level(
        GetPrefs()->GetDouble(prefs::kDefaultZoomLevel));

    const DictionaryValue* host_zoom_dictionary =
        prefs_->GetDictionary(prefs::kPerHostZoomLevels);
    // Careful: The returned value could be NULL if the pref has never been set.
    if (host_zoom_dictionary != NULL) {
      for (DictionaryValue::key_iterator i(host_zoom_dictionary->begin_keys());
           i != host_zoom_dictionary->end_keys(); ++i) {
        const std::string& host(*i);
        double zoom_level = 0;

        bool success = host_zoom_dictionary->GetDoubleWithoutPathExpansion(
            host, &zoom_level);
        DCHECK(success);
        host_zoom_map_->SetZoomLevel(host, zoom_level);
      }
    }

    registrar_.Add(this, content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
                 Source<HostZoomMap>(host_zoom_map_));
  }
  return host_zoom_map_.get();
}

GeolocationPermissionContext* ProfileImpl::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_.get())
    geolocation_permission_context_ =
        new ChromeGeolocationPermissionContext(this);
  return geolocation_permission_context_.get();
}

UserStyleSheetWatcher* ProfileImpl::GetUserStyleSheetWatcher() {
  if (!user_style_sheet_watcher_.get()) {
    user_style_sheet_watcher_ = new UserStyleSheetWatcher(GetPath());
    user_style_sheet_watcher_->Init();
  }
  return user_style_sheet_watcher_.get();
}

FindBarState* ProfileImpl::GetFindBarState() {
  if (!find_bar_state_.get()) {
    find_bar_state_.reset(new FindBarState());
  }
  return find_bar_state_.get();
}

HistoryService* ProfileImpl::GetHistoryService(ServiceAccessType sat) {
  // If saving history is disabled, only allow explicit access.
  if (GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled) &&
      sat != EXPLICIT_ACCESS)
    return NULL;

  if (!history_service_created_) {
    history_service_created_ = true;
    scoped_refptr<HistoryService> history(new HistoryService(this));
    if (!history->Init(GetPath(), GetBookmarkModel()))
      return NULL;
    history_service_.swap(history);
  }
  return history_service_.get();
}

HistoryService* ProfileImpl::GetHistoryServiceWithoutCreating() {
  return history_service_.get();
}

TemplateURLFetcher* ProfileImpl::GetTemplateURLFetcher() {
  if (!template_url_fetcher_.get())
    template_url_fetcher_.reset(new TemplateURLFetcher(this));
  return template_url_fetcher_.get();
}

AutocompleteClassifier* ProfileImpl::GetAutocompleteClassifier() {
  if (!autocomplete_classifier_.get())
    autocomplete_classifier_.reset(new AutocompleteClassifier(this));
  return autocomplete_classifier_.get();
}

history::ShortcutsBackend* ProfileImpl::GetShortcutsBackend() {
  // This is called on one thread only - UI, so no magic is needed to protect
  // against the multiple concurrent calls.
  if (!shortcuts_backend_.get()) {
    shortcuts_backend_ = new history::ShortcutsBackend(GetPath(), this);
    CHECK(shortcuts_backend_->Init());
  }
  return shortcuts_backend_.get();
}

WebDataService* ProfileImpl::GetWebDataService(ServiceAccessType sat) {
  if (!created_web_data_service_)
    CreateWebDataService();
  return web_data_service_.get();
}

WebDataService* ProfileImpl::GetWebDataServiceWithoutCreating() {
  return web_data_service_.get();
}

void ProfileImpl::CreateWebDataService() {
  DCHECK(!created_web_data_service_ && web_data_service_.get() == NULL);
  created_web_data_service_ = true;
  scoped_refptr<WebDataService> wds(new WebDataService());
  if (!wds->Init(GetPath()))
    return;
  web_data_service_.swap(wds);
}

PasswordStore* ProfileImpl::GetPasswordStore(ServiceAccessType sat) {
  if (!created_password_store_)
    CreatePasswordStore();
  return password_store_.get();
}

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
LocalProfileId ProfileImpl::GetLocalProfileId() {
  PrefService* prefs = GetPrefs();
  LocalProfileId id = prefs->GetInteger(prefs::kLocalProfileId);
  if (id == kInvalidLocalProfileId) {
    // Note that there are many more users than this. Thus, by design, this is
    // not a unique id. However, it is large enough that it is very unlikely
    // that it would be repeated twice on a single machine. It is still possible
    // for that to occur though, so the potential results of it actually
    // happening should be considered when using this value.
    static const LocalProfileId kLocalProfileIdMask =
        static_cast<LocalProfileId>((1 << 24) - 1);
    do {
      id = rand() & kLocalProfileIdMask;
      // TODO(mdm): scan other profiles to make sure they are not using this id?
    } while (id == kInvalidLocalProfileId);
    prefs->SetInteger(prefs::kLocalProfileId, id);
  }
  return id;
}
#endif  // !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)

void ProfileImpl::CreatePasswordStore() {
  DCHECK(!created_password_store_ && password_store_.get() == NULL);
  created_password_store_ = true;
  scoped_refptr<PasswordStore> ps;
  FilePath login_db_file_path = GetPath();
  login_db_file_path = login_db_file_path.Append(chrome::kLoginDataFileName);
  LoginDatabase* login_db = new LoginDatabase();
  if (!login_db->Init(login_db_file_path)) {
    LOG(ERROR) << "Could not initialize login database.";
    delete login_db;
    return;
  }
#if defined(OS_WIN)
  ps = new PasswordStoreWin(login_db, this,
                            GetWebDataService(Profile::IMPLICIT_ACCESS));
#elif defined(OS_MACOSX)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseMockKeychain)) {
    ps = new PasswordStoreMac(new MockKeychain(kMockKeychainSize), login_db);
  } else {
    ps = new PasswordStoreMac(new MacKeychain(), login_db);
  }
#elif defined(OS_CHROMEOS)
  // For now, we use PasswordStoreDefault. We might want to make a native
  // backend for PasswordStoreX (see below) in the future though.
  ps = new PasswordStoreDefault(login_db, this,
                                GetWebDataService(Profile::IMPLICIT_ACCESS));
#elif defined(OS_POSIX)
  // On POSIX systems, we try to use the "native" password management system of
  // the desktop environment currently running, allowing GNOME Keyring in XFCE.
  // (In all cases we fall back on the basic store in case of failure.)
  base::nix::DesktopEnvironment desktop_env;
  std::string store_type =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPasswordStore);
  if (store_type == "kwallet") {
    desktop_env = base::nix::DESKTOP_ENVIRONMENT_KDE4;
  } else if (store_type == "gnome") {
    desktop_env = base::nix::DESKTOP_ENVIRONMENT_GNOME;
  } else if (store_type == "basic") {
    desktop_env = base::nix::DESKTOP_ENVIRONMENT_OTHER;
  } else {
    // Detect the store to use automatically.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    desktop_env = base::nix::GetDesktopEnvironment(env.get());
    const char* name = base::nix::GetDesktopEnvironmentName(desktop_env);
    VLOG(1) << "Password storage detected desktop environment: "
            << (name ? name : "(unknown)");
  }

  scoped_ptr<PasswordStoreX::NativeBackend> backend;
  if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE4) {
    // KDE3 didn't use DBus, which our KWallet store uses.
    VLOG(1) << "Trying KWallet for password storage.";
    backend.reset(new NativeBackendKWallet(GetLocalProfileId(), GetPrefs()));
    if (backend->Init())
      VLOG(1) << "Using KWallet for password storage.";
    else
      backend.reset();
  } else if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
             desktop_env == base::nix::DESKTOP_ENVIRONMENT_XFCE) {
#if defined(USE_GNOME_KEYRING)
    VLOG(1) << "Trying GNOME keyring for password storage.";
    backend.reset(new NativeBackendGnome(GetLocalProfileId(), GetPrefs()));
    if (backend->Init())
      VLOG(1) << "Using GNOME keyring for password storage.";
    else
      backend.reset();
#endif  // defined(USE_GNOME_KEYRING)
  }

  if (!backend.get()) {
    LOG(WARNING) << "Using basic (unencrypted) store for password storage. "
        "See http://code.google.com/p/chromium/wiki/LinuxPasswordStorage for "
        "more information about password storage options.";
  }

  ps = new PasswordStoreX(login_db, this,
                          GetWebDataService(Profile::IMPLICIT_ACCESS),
                          backend.release());
#else
  NOTIMPLEMENTED();
#endif
  if (!ps)
    delete login_db;

  if (!ps || !ps->Init()) {
    NOTREACHED() << "Could not initialize password manager.";
    return;
  }
  password_store_.swap(ps);
}

DownloadManager* ProfileImpl::GetDownloadManager() {
  if (!created_download_manager_) {
    // In case the delegate has already been set by SetDownloadManagerDelegate.
    if (!download_manager_delegate_.get())
      download_manager_delegate_= new ChromeDownloadManagerDelegate(this);
    scoped_refptr<DownloadManager> dlm(
        new DownloadManager(download_manager_delegate_,
                            g_browser_process->download_status_updater()));
    dlm->Init(this);
    download_manager_delegate_->SetDownloadManager(dlm);
    created_download_manager_ = true;
    download_manager_.swap(dlm);
  }
  return download_manager_.get();
}

bool ProfileImpl::HasCreatedDownloadManager() const {
  return created_download_manager_;
}

PersonalDataManager* ProfileImpl::GetPersonalDataManager() {
  if (!personal_data_manager_.get()) {
    personal_data_manager_ = new PersonalDataManager();
    personal_data_manager_->Init(this);
  }
  return personal_data_manager_.get();
}

fileapi::FileSystemContext* ProfileImpl::GetFileSystemContext() {
  CreateQuotaManagerAndClients();
  return file_system_context_.get();
}

quota::QuotaManager* ProfileImpl::GetQuotaManager() {
  CreateQuotaManagerAndClients();
  return quota_manager_.get();
}

bool ProfileImpl::HasProfileSyncService() const {
  return (sync_service_.get() != NULL);
}

bool ProfileImpl::DidLastSessionExitCleanly() {
  // last_session_exited_cleanly_ is set when the preferences are loaded. Force
  // it to be set by asking for the prefs.
  GetPrefs();
  return last_session_exited_cleanly_;
}

BookmarkModel* ProfileImpl::GetBookmarkModel() {
  if (!bookmark_bar_model_.get()) {
    bookmark_bar_model_.reset(new BookmarkModel(this));
    bookmark_bar_model_->Load();
  }
  return bookmark_bar_model_.get();
}

ProtocolHandlerRegistry* ProfileImpl::GetProtocolHandlerRegistry() {
  return protocol_handler_registry_.get();
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

SpellCheckHost* ProfileImpl::GetSpellCheckHost() {
  return GetSpellCheckProfile()->GetHost();
}


void ProfileImpl::ReinitializeSpellCheckHost(bool force) {
  PrefService* pref = GetPrefs();
  SpellCheckProfile::ReinitializeResult result =
      GetSpellCheckProfile()->ReinitializeHost(
          force,
          pref->GetBoolean(prefs::kEnableSpellCheck),
          pref->GetString(prefs::kSpellCheckDictionary),
          GetRequestContext());
  if (result == SpellCheckProfile::REINITIALIZE_REMOVED_HOST) {
    // The spellchecker has been disabled.
    for (RenderProcessHost::iterator
         i(RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      RenderProcessHost* process = i.GetCurrentValue();
      process->Send(new SpellCheckMsg_Init(IPC::InvalidPlatformFileForTransit(),
                                           std::vector<std::string>(),
                                           std::string(),
                                           false));
    }
  }
}

ExtensionPrefValueMap* ProfileImpl::GetExtensionPrefValueMap() {
  if (!extension_pref_value_map_.get())
    extension_pref_value_map_.reset(new ExtensionPrefValueMap);
  return extension_pref_value_map_.get();
}

void ProfileImpl::CreateQuotaManagerAndClients() {
  if (quota_manager_.get()) {
    DCHECK(file_system_context_.get());
    DCHECK(db_tracker_.get());
    DCHECK(webkit_context_.get());
    return;
  }

  // All of the clients have to be created and registered with the
  // QuotaManager prior to the QuotaManger being used. So we do them
  // all together here prior to handing out a reference to anything
  // that utlizes the QuotaManager.
  quota_manager_ = new quota::QuotaManager(
      IsOffTheRecord(),
      GetPath(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
      GetExtensionSpecialStoragePolicy());

  // Each consumer is responsible for registering its QuotaClient during
  // its construction.
  file_system_context_ = CreateFileSystemContext(
      GetPath(), IsOffTheRecord(),
      GetExtensionSpecialStoragePolicy(),
      quota_manager_->proxy());
  db_tracker_ = new webkit_database::DatabaseTracker(
      GetPath(), IsOffTheRecord(), clear_local_state_on_exit_,
      GetExtensionSpecialStoragePolicy(),
      quota_manager_->proxy(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  webkit_context_ = new WebKitContext(
        IsOffTheRecord(), GetPath(), GetExtensionSpecialStoragePolicy(),
        clear_local_state_on_exit_, quota_manager_->proxy(),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::WEBKIT));
  appcache_service_ = new ChromeAppCacheService(quota_manager_->proxy());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          appcache_service_.get(),
          &ChromeAppCacheService::InitializeOnIOThread,
          IsOffTheRecord()
              ? FilePath() : GetPath().Append(chrome::kAppCacheDirname),
          &GetResourceContext(),
          make_scoped_refptr(GetExtensionSpecialStoragePolicy())));
}

WebKitContext* ProfileImpl::GetWebKitContext() {
  CreateQuotaManagerAndClients();
  return webkit_context_.get();
}

void ProfileImpl::MarkAsCleanShutdown() {
  if (prefs_.get()) {
    // The session cleanly exited, set kSessionExitedCleanly appropriately.
    prefs_->SetBoolean(prefs::kSessionExitedCleanly, true);

    // NOTE: If you change what thread this writes on, be sure and update
    // ChromeFrame::EndSession().
    prefs_->SavePersistentPrefs();
  }
}

void ProfileImpl::Observe(int type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_INITIALIZATION_COMPLETED: {
      bool* succeeded = Details<bool>(details).ptr();
      PrefService *prefs = Source<PrefService>(source).ptr();
      DCHECK(prefs == prefs_.get());
      registrar_.Remove(this,
                        chrome::NOTIFICATION_PREF_INITIALIZATION_COMPLETED,
                        Source<PrefService>(prefs));
      OnPrefsLoaded(*succeeded);
      break;
    }
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name_in = Details<std::string>(details).ptr();
      PrefService* prefs = Source<PrefService>(source).ptr();
      DCHECK(pref_name_in && prefs);
      if (*pref_name_in == prefs::kSpellCheckDictionary ||
          *pref_name_in == prefs::kEnableSpellCheck) {
        ReinitializeSpellCheckHost(true);
      } else if (*pref_name_in == prefs::kEnableAutoSpellCorrect) {
        bool enabled = prefs->GetBoolean(prefs::kEnableAutoSpellCorrect);
        for (RenderProcessHost::iterator
             i(RenderProcessHost::AllHostsIterator());
             !i.IsAtEnd(); i.Advance()) {
          RenderProcessHost* process = i.GetCurrentValue();
          process->Send(new SpellCheckMsg_EnableAutoSpellCorrect(enabled));
        }
      } else if (*pref_name_in == prefs::kSpeechInputCensorResults) {
        speech_input::ChromeSpeechInputManager::GetInstance()->
            set_censor_results(prefs->GetBoolean(
                prefs::kSpeechInputCensorResults));
      } else if (*pref_name_in == prefs::kClearSiteDataOnExit) {
        clear_local_state_on_exit_ =
            prefs->GetBoolean(prefs::kClearSiteDataOnExit);
        if (webkit_context_) {
          webkit_context_->set_clear_local_state_on_exit(
              clear_local_state_on_exit_);
        }
        if (db_tracker_) {
          db_tracker_->SetClearLocalStateOnExit(
              clear_local_state_on_exit_);
        }
      } else if (*pref_name_in == prefs::kGoogleServicesUsername) {
        ProfileManager* profile_manager = g_browser_process->profile_manager();
        profile_manager->RegisterProfileName(this);
      } else if (*pref_name_in == prefs::kDefaultZoomLevel) {
          GetHostZoomMap()->set_default_zoom_level(
              prefs->GetDouble(prefs::kDefaultZoomLevel));
      }
      break;
    }
    case chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED:
      GetProfileSyncService();  // Causes lazy-load if sync is enabled.
      registrar_.Remove(this, chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
                        Source<Profile>(this));
      break;
    case content::NOTIFICATION_ZOOM_LEVEL_CHANGED: {
      const std::string& host = *(Details<const std::string>(details).ptr());
      if (!host.empty()) {
        double level = host_zoom_map_->GetZoomLevel(host);
        DictionaryPrefUpdate update(prefs_.get(), prefs::kPerHostZoomLevels);
        DictionaryValue* host_zoom_dictionary = update.Get();
        if (level == host_zoom_map_->default_zoom_level()) {
          host_zoom_dictionary->RemoveWithoutPathExpansion(host, NULL);
        } else {
          host_zoom_dictionary->SetWithoutPathExpansion(
              host, Value::CreateDoubleValue(level));
        }
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void ProfileImpl::StopCreateSessionServiceTimer() {
  create_session_service_timer_.Stop();
}

void ProfileImpl::EnsureSessionServiceCreated() {
  SessionServiceFactory::GetForProfile(this);
}

TokenService* ProfileImpl::GetTokenService() {
  if (!token_service_.get()) {
    token_service_.reset(new TokenService());
  }
  return token_service_.get();
}

ProfileSyncService* ProfileImpl::GetProfileSyncService() {
#if defined(OS_CHROMEOS)
  if (!sync_service_.get()) {
    // In ChromeOS, sync only gets initialized properly from login, when
    // kLoginManager is specified. If this gets called before login, or
    // during a debugging session without kLoginManager, this will return
    // NULL, so ensure that calls either handle a NULL result, or use
    // HasProfileSyncService() to guard against the call.
    return NULL;
  }
#endif
  return GetProfileSyncService("");
}

ProfileSyncService* ProfileImpl::GetProfileSyncService(
    const std::string& cros_user) {

  if (!ProfileSyncService::IsSyncEnabled())
    return NULL;
  if (!sync_service_.get())
    InitSyncService(cros_user);
  return sync_service_.get();
}

void ProfileImpl::InitSyncService(const std::string& cros_user) {
  profile_sync_factory_.reset(
      new ProfileSyncFactoryImpl(this, CommandLine::ForCurrentProcess()));
  sync_service_.reset(
      profile_sync_factory_->CreateProfileSyncService(cros_user));
  profile_sync_factory_->RegisterDataTypes(sync_service_.get());
  sync_service_->Initialize();
}

ChromeBlobStorageContext* ProfileImpl::GetBlobStorageContext() {
  if (!blob_storage_context_) {
    blob_storage_context_ = new ChromeBlobStorageContext();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(blob_storage_context_.get(),
                          &ChromeBlobStorageContext::InitializeOnIOThread));
  }
  return blob_storage_context_;
}

ExtensionInfoMap* ProfileImpl::GetExtensionInfoMap() {
  return extension_info_map_.get();
}

ChromeURLDataManager* ProfileImpl::GetChromeURLDataManager() {
  if (!chrome_url_data_manager_.get())
    chrome_url_data_manager_.reset(new ChromeURLDataManager(
        io_data_.GetChromeURLDataManagerBackendGetter()));
  return chrome_url_data_manager_.get();
}

PromoCounter* ProfileImpl::GetInstantPromoCounter() {
#if defined(OS_WIN)
  // TODO: enable this when we're ready to turn on the promo.
  /*
  if (!checked_instant_promo_) {
    checked_instant_promo_ = true;
    PrefService* prefs = GetPrefs();
    if (!prefs->GetBoolean(prefs::kInstantEnabledOnce) &&
        !InstantController::IsEnabled(this) &&
        InstallUtil::IsChromeSxSProcess()) {
      DCHECK(!instant_promo_counter_.get());
      instant_promo_counter_.reset(
          new PromoCounter(this, prefs::kInstantPromo, "Instant.Promo", 3, 3));
    }
  }
  */
  return instant_promo_counter_.get();
#else
  return NULL;
#endif
}

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

  if (chromeos::UserManager::Get()->current_user_is_owner())
    local_state->SetString(prefs::kOwnerLocale, new_locale);

  GetPrefs()->ScheduleSavePersistentPrefs();
  local_state->ScheduleSavePersistentPrefs();
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
  chromeos_preferences_->Init(GetPrefs());
}
#endif  // defined(OS_CHROMEOS)

PrefProxyConfigTracker* ProfileImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_)
    pref_proxy_config_tracker_ = new PrefProxyConfigTracker(GetPrefs());

  return pref_proxy_config_tracker_;
}

prerender::PrerenderManager* ProfileImpl::GetPrerenderManager() {
  if (!prerender::PrerenderManager::IsPrerenderingPossible())
    return NULL;
  if (!prerender_manager_.get()) {
    CHECK(g_browser_process->prerender_tracker());
    prerender_manager_.reset(
        new prerender::PrerenderManager(
            this, g_browser_process->prerender_tracker()));
#if defined(OS_CHROMEOS)
    prerender_manager_->AddCondition(
        new chromeos::PrerenderConditionNetwork(
            chromeos::CrosLibrary::Get()->GetNetworkLibrary()));
#endif
  }
  return prerender_manager_.get();
}

chrome_browser_net::Predictor* ProfileImpl::GetNetworkPredictor() {
  return predictor_;
}

SpellCheckProfile* ProfileImpl::GetSpellCheckProfile() {
  if (!spellcheck_profile_.get())
    spellcheck_profile_.reset(new SpellCheckProfile());
  return spellcheck_profile_.get();
}

void ProfileImpl::SetDownloadManagerDelegate(
    ChromeDownloadManagerDelegate* delegate) {
  download_manager_delegate_ = delegate;
}
