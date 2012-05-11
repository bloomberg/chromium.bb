// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/speech/chrome_speech_recognition_preferences.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/user_style_sheet_watcher.h"
#include "chrome/browser/visitedlink/visitedlink_event_listener.h"
#include "chrome/browser/visitedlink/visitedlink_master.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/installer/util/install_util.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/enterprise_extension_observer.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/locale_change_guard.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#endif

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using content::DownloadManager;
using content::HostZoomMap;
using content::UserMetricsAction;

namespace {

// Constrict us to a very specific platform and architecture to make sure
// ifdefs don't cause problems with the check.
#if defined(OS_LINUX) && defined(TOOLKIT_GTK) && defined(ARCH_CPU_X86_64)
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

// Helper method needed because PostTask cannot currently take a Callback
// function with non-void return type.
// TODO(jhawkins): Remove once IgnoreResult is fixed.
void CreateDirectoryNoResult(const FilePath& path) {
  file_util::CreateDirectory(path);
}

FilePath GetCachePath(const FilePath& base) {
  return base.Append(chrome::kCacheDirname);
}

FilePath GetMediaCachePath(const FilePath& base) {
  return base.Append(chrome::kMediaCacheDirname);
}

void EnsureReadmeFile(const FilePath& base) {
  FilePath readme_path = base.Append(chrome::kReadmeFilename);
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

}  // namespace

// static
Profile* Profile::CreateProfile(const FilePath& path,
                                Delegate* delegate,
                                CreateMode create_mode) {
  if (create_mode == CREATE_MODE_ASYNCHRONOUS) {
    DCHECK(delegate);
    // This is safe while all file operations are done on the FILE thread.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CreateDirectoryNoResult, path));
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

  return new ProfileImpl(path, delegate, create_mode);
}

// static
int ProfileImpl::create_readme_delay_ms = 60000;

// static
void ProfileImpl::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kClearSiteDataOnExit,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kProfileShortcutCreated,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kProfileAvatarIndex,
                             -1,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kProfileName,
                            "",
                            PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kRestoreSessionStateDialogShown,
                             false,
                             PrefService::SYNCABLE_PREF);
}

ProfileImpl::ProfileImpl(const FilePath& path,
                         Delegate* delegate,
                         CreateMode create_mode)
    : path_(path),
      ALLOW_THIS_IN_INITIALIZER_LIST(visited_link_event_listener_(
          new VisitedLinkEventListener(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_data_(this)),
      host_content_settings_map_(NULL),
      history_service_created_(false),
      favicon_service_created_(false),
      clear_local_state_on_exit_(false),
      start_time_(Time::Now()),
      delegate_(delegate),
      predictor_(NULL),
      session_restore_enabled_(false) {
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

  session_restore_enabled_ =
      !command_line->HasSwitch(switches::kDisableRestoreSessionState);
  if (create_mode == CREATE_MODE_ASYNCHRONOUS) {
    prefs_.reset(PrefService::CreatePrefService(
        GetPrefFilePath(),
        new ExtensionPrefStore(
            ExtensionPrefValueMapFactory::GetForProfile(this), false),
        true));
    // Wait for the notification that prefs has been loaded (successfully or
    // not).
    registrar_.Add(this, chrome::NOTIFICATION_PREF_INITIALIZATION_COMPLETED,
                   content::Source<PrefService>(prefs_.get()));
  } else if (create_mode == CREATE_MODE_SYNCHRONOUS) {
    // Load prefs synchronously.
    prefs_.reset(PrefService::CreatePrefService(
        GetPrefFilePath(),
        new ExtensionPrefStore(
            ExtensionPrefValueMapFactory::GetForProfile(this), false),
        false));
    OnPrefsLoaded(true);
  } else {
    NOTREACHED();
  }
}

void ProfileImpl::DoFinalInit(bool is_new_profile) {
  PrefService* prefs = GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kSpeechRecognitionFilterProfanities, this);
  pref_change_registrar_.Add(prefs::kClearSiteDataOnExit, this);
  pref_change_registrar_.Add(prefs::kGoogleServicesUsername, this);
  pref_change_registrar_.Add(prefs::kDefaultZoomLevel, this);
  pref_change_registrar_.Add(prefs::kProfileAvatarIndex, this);
  pref_change_registrar_.Add(prefs::kProfileName, this);

  // It would be nice to use PathService for fetching this directory, but
  // the cache directory depends on the profile directory, which isn't available
  // to PathService.
  chrome::GetUserCacheDirectory(path_, &base_cache_path_);
  // Always create the cache directory asynchronously.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateDirectoryNoResult, base_cache_path_));

  // Now that the profile is hooked up to receive pref change notifications to
  // kGoogleServicesUsername, initialize components that depend on it to reflect
  // the current value.
  UpdateProfileUserNameCache();
  GetGAIAInfoUpdateService();

#if !defined(OS_CHROMEOS)
  // Listen for bookmark model load, to bootstrap the sync service.
  // On CrOS sync service will be initialized after sign in.
  registrar_.Add(this, chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
                 content::Source<Profile>(this));
#endif

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

  InitRegisteredProtocolHandlers();

  clear_local_state_on_exit_ = prefs->GetBoolean(prefs::kClearSiteDataOnExit);
  if (clear_local_state_on_exit_) {
    content::RecordAction(
        UserMetricsAction("ClearSiteDataOnExitEnabled"));
  } else {
    content::RecordAction(
        UserMetricsAction("ClearSiteDataOnExitDisabled"));
  }

  InstantController::RecordMetrics(this);

  FilePath cookie_path = GetPath();
  cookie_path = cookie_path.Append(chrome::kCookieFilename);
  FilePath server_bound_cert_path = GetPath();
  server_bound_cert_path =
      server_bound_cert_path.Append(chrome::kOBCertFilename);
  FilePath cache_path = base_cache_path_;
  int cache_max_size;
  GetCacheParameters(false, &cache_path, &cache_max_size);
  cache_path = GetCachePath(cache_path);

  FilePath media_cache_path = base_cache_path_;
  int media_cache_max_size;
  GetCacheParameters(true, &media_cache_path, &media_cache_max_size);
  media_cache_path = GetMediaCachePath(media_cache_path);

  FilePath extensions_cookie_path = GetPath();
  extensions_cookie_path =
      extensions_cookie_path.Append(chrome::kExtensionsCookieFilename);

  FilePath app_path = GetPath().Append(chrome::kIsolatedAppStateDirname);

#if defined(OS_ANDROID)
  SessionStartupPref::Type startup_pref_type =
      SessionStartupPref::GetDefaultStartupType();
#else
  SessionStartupPref::Type startup_pref_type =
      StartupBrowserCreator::GetSessionStartupPref(
          *CommandLine::ForCurrentProcess(), this).type;
#endif
  bool restore_old_session_cookies =
      session_restore_enabled_ &&
      (!DidLastSessionExitCleanly() ||
       startup_pref_type == SessionStartupPref::LAST);

  InitHostZoomMap();

  // Make sure we initialize the ProfileIOData after everything else has been
  // initialized that we might be reading from the IO thread.

  io_data_.Init(cookie_path, server_bound_cert_path, cache_path,
                cache_max_size, media_cache_path, media_cache_max_size,
                extensions_cookie_path, app_path, predictor_,
                g_browser_process->local_state(),
                g_browser_process->io_thread(),
                restore_old_session_cookies);

  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      PluginPrefs::GetForProfile(this),
      io_data_.GetResourceContextNoInit());

  // Delay README creation to not impact startup performance.
  BrowserThread::PostDelayedTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&EnsureReadmeFile, GetPath()),
        base::TimeDelta::FromMilliseconds(create_readme_delay_ms));

  // Creation has been finished.
  if (delegate_)
    delegate_->OnProfileCreated(this, true, is_new_profile);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(this),
      content::NotificationService::NoDetails());
}

void ProfileImpl::InitHostZoomMap() {
  HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);
  host_zoom_map->SetDefaultZoomLevel(
      prefs_->GetDouble(prefs::kDefaultZoomLevel));

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
      host_zoom_map->SetZoomLevel(host, zoom_level);
    }
  }

  registrar_.Add(this, content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
               content::Source<HostZoomMap>(host_zoom_map));
}

void ProfileImpl::InitPromoResources() {
#if defined(ENABLE_PROMO_RESOURCE_SERVICE)
  if (promo_resource_service_)
    return;

  promo_resource_service_ = new PromoResourceService(this);
  promo_resource_service_->StartAfterDelay();
#endif
}

void ProfileImpl::InitRegisteredProtocolHandlers() {
  if (protocol_handler_registry_)
    return;
  protocol_handler_registry_ = new ProtocolHandlerRegistry(this,
      new ProtocolHandlerRegistry::Delegate());

  // Install predefined protocol handlers.
  InstallDefaultProtocolHandlers();

  protocol_handler_registry_->Load();
}

void ProfileImpl::InstallDefaultProtocolHandlers() {
#if defined(OS_CHROMEOS)
  protocol_handler_registry_->AddPredefinedHandler(
      ProtocolHandler::CreateProtocolHandler(
          "mailto",
          GURL(l10n_util::GetStringUTF8(IDS_GOOGLE_MAILTO_HANDLER_URL)),
          l10n_util::GetStringUTF16(IDS_GOOGLE_MAILTO_HANDLER_NAME)));
  protocol_handler_registry_->AddPredefinedHandler(
      ProtocolHandler::CreateProtocolHandler(
          "webcal",
          GURL(l10n_util::GetStringUTF8(IDS_GOOGLE_WEBCAL_HANDLER_URL)),
          l10n_util::GetStringUTF16(IDS_GOOGLE_WEBCAL_HANDLER_NAME)));
#endif
}

FilePath ProfileImpl::last_selected_directory() {
  return GetPrefs()->GetFilePath(prefs::kSelectFileLastDirectory);
}

void ProfileImpl::set_last_selected_directory(const FilePath& path) {
  GetPrefs()->SetFilePath(prefs::kSelectFileLastDirectory, path);
}

ProfileImpl::~ProfileImpl() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::Source<Profile>(this),
      content::NotificationService::NoDetails());
  bool prefs_loaded = prefs_->GetInitializationStatus() !=
      PrefService::INITIALIZATION_STATUS_WAITING;
  // Honor the "clear local state" setting. If it's not set, keep the session
  // data if we're going to continue the session upon startup.
  if (clear_local_state_on_exit_) {
    BrowserContext::ClearLocalOnDestruction(this);
  } else if (session_restore_enabled_ && prefs_loaded) {
    SessionStartupPref pref = SessionStartupPref::GetStartupPref(this);
    if (pref.type == SessionStartupPref::LAST)
      BrowserContext::SaveSessionState(this);
  }

#if defined(ENABLE_SESSION_SERVICE)
  StopCreateSessionServiceTimer();
#endif

  // Remove pref observers
  pref_change_registrar_.RemoveAll();

  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
      io_data_.GetResourceContextNoInit());

  if (io_data_.HasMainRequestContext() &&
      default_request_context_ == GetRequestContext()) {
    default_request_context_ = NULL;
  }

  // Destroy OTR profile and its profile services first.
  DestroyOffTheRecordProfile();

  ProfileDependencyManager::GetInstance()->DestroyProfileServices(this);

  // The HistoryService maintains threads for background processing. Its
  // possible each thread still has tasks on it that have increased the ref
  // count of the service. In such a situation, when we decrement the refcount,
  // it won't be 0, and the threads/databases aren't properly shut down. By
  // explicitly calling Cleanup/Shutdown we ensure the databases are properly
  // closed.

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

  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();

  if (protocol_handler_registry_)
    protocol_handler_registry_->Finalize();

  if (host_content_settings_map_)
    host_content_settings_map_->ShutdownOnUIThread();

  // This causes the Preferences file to be written to disk.
  if (prefs_loaded)
    MarkAsCleanShutdown();
}

std::string ProfileImpl::GetProfileName() {
  return GetPrefs()->GetString(prefs::kGoogleServicesUsername);
}

FilePath ProfileImpl::GetPath() {
  return path_;
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
  return ExtensionSystem::Get(this)->extension_service();
}

UserScriptMaster* ProfileImpl::GetUserScriptMaster() {
  return ExtensionSystem::Get(this)->user_script_master();
}

ExtensionProcessManager* ProfileImpl::GetExtensionProcessManager() {
  return ExtensionSystem::Get(this)->process_manager();
}

ExtensionEventRouter* ProfileImpl::GetExtensionEventRouter() {
  return ExtensionSystem::Get(this)->event_router();
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

  // The Profile class and ProfileManager class may read some prefs so
  // register known prefs as soon as possible.
  Profile::RegisterUserPrefs(prefs_.get());
  browser::RegisterUserPrefs(prefs_.get());
  // TODO(mirandac): remove migration code after 6 months (crbug.com/69995).
  if (g_browser_process->local_state())
    browser::MigrateBrowserPrefs(this, g_browser_process->local_state());

  // The last session exited cleanly if there is no pref for
  // kSessionExitedCleanly or the value for kSessionExitedCleanly is true.
  last_session_exited_cleanly_ =
      prefs_->GetBoolean(prefs::kSessionExitedCleanly);
  // Mark the session as open.
  prefs_->SetBoolean(prefs::kSessionExitedCleanly, false);

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

PrefService* ProfileImpl::GetPrefs() {
  DCHECK(prefs_.get());  // Should explicitly be initialized.
  return prefs_.get();
}

PrefService* ProfileImpl::GetOffTheRecordPrefs() {
  if (!otr_prefs_.get()) {
    // The new ExtensionPrefStore is ref_counted and the new PrefService
    // stores a reference so that we do not leak memory here.
    otr_prefs_.reset(GetPrefs()->CreateIncognitoPrefService(
        new ExtensionPrefStore(
            ExtensionPrefValueMapFactory::GetForProfile(this), true)));
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
  ExtensionService* extension_service =
      ExtensionSystem::Get(this)->extension_service();
  if (extension_service) {
    const Extension* installed_app = extension_service->
        GetInstalledAppForRenderer(renderer_child_id);
    if (installed_app != NULL && installed_app->is_storage_isolated()) {
      return GetRequestContextForIsolatedApp(installed_app->id());
    }
  }
  return GetRequestContext();
}

net::URLRequestContextGetter* ProfileImpl::GetRequestContextForMedia() {
  return io_data_.GetMediaRequestContextGetter();
}

content::ResourceContext* ProfileImpl::GetResourceContext() {
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

content::GeolocationPermissionContext*
    ProfileImpl::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_.get()) {
    geolocation_permission_context_ =
        new ChromeGeolocationPermissionContext(this);
  }
  return geolocation_permission_context_.get();
}

content::SpeechRecognitionPreferences*
    ProfileImpl::GetSpeechRecognitionPreferences() {
#if defined(ENABLE_INPUT_SPEECH)
  if (!speech_recognition_preferences_.get()) {
    speech_recognition_preferences_ =
        new ChromeSpeechRecognitionPreferences(GetPrefs());
  }
  return speech_recognition_preferences_.get();
#else
  return NULL;
#endif
}

GAIAInfoUpdateService* ProfileImpl::GetGAIAInfoUpdateService() {
  if (!gaia_info_update_service_.get() &&
      GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(this)) {
    gaia_info_update_service_.reset(new GAIAInfoUpdateService(this));
  }
  return gaia_info_update_service_.get();
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

DownloadManager* ProfileImpl::GetDownloadManager() {
  return DownloadServiceFactory::GetForProfile(this)->GetDownloadManager();
}

bool ProfileImpl::DidLastSessionExitCleanly() {
  // last_session_exited_cleanly_ is set when the preferences are loaded. Force
  // it to be set by asking for the prefs.
  GetPrefs();
  return last_session_exited_cleanly_;
}

quota::SpecialStoragePolicy* ProfileImpl::GetSpecialStoragePolicy() {
  return GetExtensionSpecialStoragePolicy();
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

void ProfileImpl::MarkAsCleanShutdown() {
  if (prefs_.get()) {
    // The session cleanly exited, set kSessionExitedCleanly appropriately.
    prefs_->SetBoolean(prefs::kSessionExitedCleanly, true);

    // NOTE: If you change what thread this writes on, be sure and update
    // ChromeFrame::EndSession().
    prefs_->CommitPendingWrite();
  }
}

void ProfileImpl::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_INITIALIZATION_COMPLETED: {
      bool* succeeded = content::Details<bool>(details).ptr();
      PrefService *prefs = content::Source<PrefService>(source).ptr();
      DCHECK(prefs == prefs_.get());
      registrar_.Remove(this,
                        chrome::NOTIFICATION_PREF_INITIALIZATION_COMPLETED,
                        content::Source<PrefService>(prefs));
      OnPrefsLoaded(*succeeded);
      break;
    }
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name_in = content::Details<std::string>(details).ptr();
      PrefService* prefs = content::Source<PrefService>(source).ptr();
      DCHECK(pref_name_in && prefs);
      if (*pref_name_in == prefs::kSpeechRecognitionFilterProfanities) {
        content::SpeechRecognitionPreferences* speech_prefs =
            GetSpeechRecognitionPreferences();
        if (speech_prefs) {
          speech_prefs->SetFilterProfanities(prefs->GetBoolean(
              prefs::kSpeechRecognitionFilterProfanities));
        }
      } else if (*pref_name_in == prefs::kClearSiteDataOnExit) {
        clear_local_state_on_exit_ =
            prefs->GetBoolean(prefs::kClearSiteDataOnExit);
      } else if (*pref_name_in == prefs::kGoogleServicesUsername) {
        UpdateProfileUserNameCache();
      } else if (*pref_name_in == prefs::kProfileAvatarIndex) {
        UpdateProfileAvatarCache();
      } else if (*pref_name_in == prefs::kProfileName) {
        UpdateProfileNameCache();
      } else if (*pref_name_in == prefs::kDefaultZoomLevel) {
          HostZoomMap::GetForBrowserContext(this)->SetDefaultZoomLevel(
              prefs->GetDouble(prefs::kDefaultZoomLevel));
      }
      break;
    }
    case chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED:
      // Causes lazy-load if sync is enabled.
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(this);
      registrar_.Remove(this, chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
                        content::Source<Profile>(this));
      break;
    case content::NOTIFICATION_ZOOM_LEVEL_CHANGED: {
      const std::string& host =
          *(content::Details<const std::string>(details).ptr());
      if (!host.empty()) {
        HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);
        double level = host_zoom_map->GetZoomLevel(host);
        DictionaryPrefUpdate update(prefs_.get(), prefs::kPerHostZoomLevels);
        DictionaryValue* host_zoom_dictionary = update.Get();
        if (level == host_zoom_map->GetDefaultZoomLevel()) {
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
  chromeos_preferences_->Init(GetPrefs());
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

void ProfileImpl::ClearNetworkingHistorySince(base::Time time) {
  io_data_.ClearNetworkingHistorySince(time);
}

GURL ProfileImpl::GetHomePage() {
  // --homepage overrides any preferences.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kHomePage)) {
    // TODO(evanm): clean up usage of DIR_CURRENT.
    //   http://code.google.com/p/chromium/issues/detail?id=60630
    // For now, allow this code to call getcwd().
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    FilePath browser_directory;
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
                                     FilePath* cache_path,
                                     int* max_size) {
  DCHECK(cache_path);
  DCHECK(max_size);
  FilePath path(prefs_->GetFilePath(prefs::kDiskCacheDir));
  if (!path.empty())
    *cache_path = path;
  *max_size = is_media_context ? prefs_->GetInteger(prefs::kMediaCacheSize) :
                                 prefs_->GetInteger(prefs::kDiskCacheSize);
}

base::Callback<ChromeURLDataManagerBackend*(void)>
    ProfileImpl::GetChromeURLDataManagerBackendGetter() const {
  return io_data_.GetChromeURLDataManagerBackendGetter();
}
