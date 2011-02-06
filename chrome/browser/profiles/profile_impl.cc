// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/appcache/chrome_appcache_service.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_signin.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chrome_blob_storage_context.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/dom_ui/ntp_resource_cache.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/extensions/default_apps.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_io_event_router.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/file_system/browser_file_system_helper.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/net/net_pref_observer.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/profile_policy_context.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/spellcheck_host.h"
#include "chrome/browser/ssl/ssl_host_state.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/sync/profile_sync_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tabs/pinned_tab_service.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/transport_security_persister.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/user_style_sheet_watcher.h"
#include "chrome/browser/visitedlink/visitedlink_event_listener.h"
#include "chrome/browser/visitedlink/visitedlink_master.h"
#include "chrome/browser/web_resource/web_resource_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "grit/browser_resources.h"
#include "grit/locale_settings.h"
#include "net/base/transport_security_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/database/database_tracker.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/instant/promo_counter.h"
#include "chrome/browser/password_manager/password_store_win.h"
#include "chrome/installer/util/install_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/keychain_mac.h"
#include "chrome/browser/password_manager/password_store_mac.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/enterprise_extension_observer.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#elif defined(OS_POSIX) && !defined(OS_CHROMEOS)
#include "base/nix/xdg_util.h"
#if defined(USE_GNOME_KEYRING)
#include "chrome/browser/password_manager/native_backend_gnome_x.h"
#endif
#include "chrome/browser/password_manager/native_backend_kwallet_x.h"
#include "chrome/browser/password_manager/password_store_x.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/preferences.h"
#endif

using base::Time;
using base::TimeDelta;

namespace {

// Delay, in milliseconds, before we explicitly create the SessionService.
static const int kCreateSessionServiceDelayMS = 500;

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

  const char* arg = kNormalContext == type ? switches::kDiskCacheSize :
                                             switches::kMediaCacheSize;
  std::string value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(arg);

  // By default we let the cache determine the right size.
  *max_size = 0;
  if (!base::StringToInt(value, max_size)) {
    *max_size = 0;
  } else if (max_size < 0) {
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
  return new ProfileImpl(path);
}

// static
void ProfileImpl::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled, false);
  DefaultApps::RegisterUserPrefs(prefs);
}

ProfileImpl::ProfileImpl(const FilePath& path)
    : path_(path),
      visited_link_event_listener_(new VisitedLinkEventListener()),
      extension_devtools_manager_(NULL),
      host_content_settings_map_(NULL),
      host_zoom_map_(NULL),
      history_service_created_(false),
      favicon_service_created_(false),
      created_web_data_service_(false),
      created_password_store_(false),
      created_download_manager_(false),
      created_theme_provider_(false),
      start_time_(Time::Now()),
      spellcheck_host_(NULL),
      spellcheck_host_ready_(false),
#if defined(OS_WIN)
      checked_instant_promo_(false),
#endif
      shutdown_session_service_(false) {
  DCHECK(!path.empty()) << "Using an empty path will attempt to write " <<
                            "profile files to the root directory!";
  create_session_service_timer_.Start(
      TimeDelta::FromMilliseconds(kCreateSessionServiceDelayMS), this,
      &ProfileImpl::EnsureSessionServiceCreated);

  PrefService* prefs = GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kSpellCheckDictionary, this);
  pref_change_registrar_.Add(prefs::kEnableSpellCheck, this);
  pref_change_registrar_.Add(prefs::kEnableAutoSpellCorrect, this);
  pref_change_registrar_.Add(prefs::kClearSiteDataOnExit, this);

  // It would be nice to use PathService for fetching this directory, but
  // the cache directory depends on the profile directory, which isn't available
  // to PathService.
  chrome::GetUserCacheDirectory(path_, &base_cache_path_);
  file_util::CreateDirectory(base_cache_path_);

  FilePath cookie_path = GetPath();
  cookie_path = cookie_path.Append(chrome::kCookieFilename);
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

  io_data_.Init(cookie_path, cache_path, cache_max_size,
                media_cache_path, media_cache_max_size, extensions_cookie_path,
                this);

  // Listen for theme installations from our original profile.
  registrar_.Add(this, NotificationType::THEME_INSTALLED,
                 Source<Profile>(GetOriginalProfile()));

#if !defined(OS_CHROMEOS)
  // Listen for bookmark model load, to bootstrap the sync service.
  // On CrOS sync service will be initialized after sign in.
  registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                 Source<Profile>(this));
#endif

  ssl_config_service_manager_.reset(
      SSLConfigServiceManager::CreateDefaultManager(this));

#if defined(OS_CHROMEOS)
  chromeos_preferences_.reset(new chromeos::Preferences());
  chromeos_preferences_->Init(prefs);
#endif

  pinned_tab_service_.reset(new PinnedTabService(this));

  // Initialize the BackgroundModeManager - this has to be done here before
  // InitExtensions() is called because it relies on receiving notifications
  // when extensions are loaded. BackgroundModeManager is not needed under
  // ChromeOS because Chrome is always running (no need for special keep-alive
  // or launch-on-startup support).
#if !defined(OS_CHROMEOS)
  background_mode_manager_.reset(new BackgroundModeManager(this,
      CommandLine::ForCurrentProcess()));
#endif

  background_contents_service_.reset(
      new BackgroundContentsService(this, CommandLine::ForCurrentProcess()));

  extension_io_event_router_ = new ExtensionIOEventRouter(this);
  extension_info_map_ = new ExtensionInfoMap();

  GetPolicyContext()->Initialize();

  clear_local_state_on_exit_ = prefs->GetBoolean(prefs::kClearSiteDataOnExit);

  // Log the profile size after a reasonable startup delay.
  BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE,
                                 new ProfileSizeTask(path_), 112000);

  InstantController::RecordMetrics(this);
}

void ProfileImpl::InitExtensions() {
  if (user_script_master_ || extensions_service_)
    return;  // Already initialized.

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
      switches::kEnableExtensionTimelineApi)) {
    extension_devtools_manager_ = new ExtensionDevToolsManager(this);
  }

  extension_process_manager_.reset(ExtensionProcessManager::Create(this));
  extension_event_router_.reset(new ExtensionEventRouter(this));
  extension_io_event_router_ = new ExtensionIOEventRouter(this);
  extension_message_service_ = new ExtensionMessageService(this);

  ExtensionErrorReporter::Init(true);  // allow noisy errors.

  FilePath script_dir;  // Don't look for user scripts in any directory.
                        // TODO(aa): We should just remove this functionality,
                        // since it isn't used anymore.
  user_script_master_ = new UserScriptMaster(script_dir, this);

  extensions_service_ = new ExtensionService(
      this,
      CommandLine::ForCurrentProcess(),
      GetPath().AppendASCII(ExtensionService::kInstallDirectoryName),
      extension_prefs_.get(),
      true);

  RegisterComponentExtensions();
  extensions_service_->Init();
  InstallDefaultApps();

  // Load any extensions specified with --load-extension.
  if (command_line->HasSwitch(switches::kLoadExtension)) {
    FilePath path = command_line->GetSwitchValuePath(switches::kLoadExtension);
    extensions_service_->LoadExtension(path);
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
  //   rsa -pubout -outform DER < /tmp/key.pem 2>/dev/null | base64 -w 0
  typedef std::list<std::pair<FilePath::StringType, int> >
      ComponentExtensionList;
  ComponentExtensionList component_extensions;

  // Bookmark manager.
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("bookmark_manager"),
      IDR_BOOKMARKS_MANIFEST));

#if defined(TOUCH_UI)
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("keyboard"),
      IDR_KEYBOARD_MANIFEST));
#endif

#if defined(OS_CHROMEOS) && defined(OFFICIAL_BUILD)
  if (browser_defaults::enable_help_app) {
    component_extensions.push_back(std::make_pair(
        FILE_PATH_LITERAL("/usr/share/chromeos-assets/helpapp"),
        IDR_HELP_MANIFEST));
  }

  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("/usr/share/chromeos-assets/getstarted"),
      IDR_GETSTARTED_MANIFEST));

  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("/usr/share/chromeos-assets/mobile"),
      IDR_MOBILE_MANIFEST));
#endif

  // Web Store.
  component_extensions.push_back(std::make_pair(
      FILE_PATH_LITERAL("web_store"),
      IDR_WEBSTORE_MANIFEST));

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
    extensions_service_->register_component_extension(
        ExtensionService::ComponentExtensionInfo(manifest, path));
  }

#if defined(OS_CHROMEOS)
  // Register access extensions only if accessibility is enabled.
  if (g_browser_process->local_state()->
      GetBoolean(prefs::kAccessibilityEnabled)) {
    FilePath path = FilePath(extension_misc::kAccessExtensionPath)
        .AppendASCII("access_chromevox");
    std::string manifest =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_CHROMEVOX_MANIFEST).as_string();
    extensions_service_->register_component_extension(
        ExtensionService::ComponentExtensionInfo(manifest, path));
  }
#endif
}

void ProfileImpl::InstallDefaultApps() {
  ExtensionService* extension_service = GetExtensionService();
  DefaultApps* default_apps = extension_service->default_apps();

  if (!default_apps->ShouldInstallDefaultApps(extension_service->GetAppIds()))
    return;

  const ExtensionIdSet& app_ids = default_apps->default_apps();
  for (ExtensionIdSet::const_iterator iter = app_ids.begin();
       iter != app_ids.end(); ++iter) {
    extension_service->AddPendingExtensionFromDefaultAppList(*iter);
  }
}

void ProfileImpl::InitWebResources() {
  if (web_resource_service_)
    return;

  web_resource_service_ = new WebResourceService(this);
  web_resource_service_->StartAfterDelay();
}

NTPResourceCache* ProfileImpl::GetNTPResourceCache() {
  if (!ntp_resource_cache_.get())
    ntp_resource_cache_.reset(new NTPResourceCache(this));
  return ntp_resource_cache_.get();
}

FilePath ProfileImpl::last_selected_directory() {
  return GetPrefs()->GetFilePath(prefs::kSelectFileLastDirectory);
}

void ProfileImpl::set_last_selected_directory(const FilePath& path) {
  GetPrefs()->SetFilePath(prefs::kSelectFileLastDirectory, path);
}

ProfileImpl::~ProfileImpl() {
  NotificationService::current()->Notify(
      NotificationType::PROFILE_DESTROYED,
      Source<Profile>(this),
      NotificationService::NoDetails());

  GetPolicyContext()->Shutdown();

  tab_restore_service_ = NULL;

  StopCreateSessionServiceTimer();
  // TemplateURLModel schedules a task on the WebDataService from its
  // destructor. Delete it first to ensure the task gets scheduled before we
  // shut down the database.
  template_url_model_.reset();

  // DownloadManager is lazily created, so check before accessing it.
  if (download_manager_.get()) {
    // The download manager queries the history system and should be shut down
    // before the history is shut down so it can properly cancel all requests.
    download_manager_->Shutdown();
    download_manager_ = NULL;
  }

  // The theme provider provides bitmaps to whoever wants them.
  theme_provider_.reset();

  // Remove pref observers
  pref_change_registrar_.RemoveAll();

  // Delete the NTP resource cache so we can unregister pref observers.
  ntp_resource_cache_.reset();

  // The sync service needs to be deleted before the services it calls.
  sync_service_.reset();

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

  if (history_service_.get())
    history_service_->Cleanup();

  if (spellcheck_host_.get())
    spellcheck_host_->UnsetObserver();

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
  favicon_service_ = NULL;

  if (extension_io_event_router_)
    extension_io_event_router_->DestroyingProfile();

  if (extension_message_service_)
    extension_message_service_->DestroyingProfile();

  if (extensions_service_)
    extensions_service_->DestroyingProfile();

  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();

  // This causes the Preferences file to be written to disk.
  MarkAsCleanShutdown();
}

ProfileId ProfileImpl::GetRuntimeId() {
  return reinterpret_cast<ProfileId>(this);
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
        NotificationType::OTR_PROFILE_CREATED,
        Source<Profile>(off_the_record_profile_.get()),
        NotificationService::NoDetails());
  }
  return off_the_record_profile_.get();
}

void ProfileImpl::DestroyOffTheRecordProfile() {
  off_the_record_profile_.reset();
}

bool ProfileImpl::HasOffTheRecordProfile() {
  return off_the_record_profile_.get() != NULL;
}

Profile* ProfileImpl::GetOriginalProfile() {
  return this;
}

ChromeAppCacheService* ProfileImpl::GetAppCacheService() {
  if (!appcache_service_) {
    appcache_service_ = new ChromeAppCacheService;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(appcache_service_.get(),
                          &ChromeAppCacheService::InitializeOnIOThread,
                          GetPath(), IsOffTheRecord(),
                          make_scoped_refptr(GetHostContentSettingsMap()),
                          clear_local_state_on_exit_));
  }
  return appcache_service_;
}

webkit_database::DatabaseTracker* ProfileImpl::GetDatabaseTracker() {
  if (!db_tracker_) {
    db_tracker_ = new webkit_database::DatabaseTracker(
        GetPath(), IsOffTheRecord());
  }
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
  return extensions_service_.get();
}

BackgroundContentsService* ProfileImpl::GetBackgroundContentsService() const {
  return background_contents_service_.get();
}

StatusTray* ProfileImpl::GetStatusTray() {
  if (!status_tray_.get())
    status_tray_.reset(StatusTray::Create());
  return status_tray_.get();
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

ExtensionIOEventRouter* ProfileImpl::GetExtensionIOEventRouter() {
  return extension_io_event_router_.get();
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
    transport_security_state_ = new net::TransportSecurityState();
    transport_security_persister_ =
        new TransportSecurityPersister();
    transport_security_persister_->Initialize(
        transport_security_state_.get(), path_);
  }

  return transport_security_state_.get();
}

PrefService* ProfileImpl::GetPrefs() {
  if (!prefs_.get()) {
    prefs_.reset(PrefService::CreatePrefService(
        GetPrefFilePath(),
        new ExtensionPrefStore(GetExtensionPrefValueMap(), false),
        GetOriginalProfile()));

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

    DCHECK(!net_pref_observer_.get());
    net_pref_observer_.reset(new NetPrefObserver(prefs_.get()));
  }

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

URLRequestContextGetter* ProfileImpl::GetRequestContext() {
  URLRequestContextGetter* request_context =
      io_data_.GetMainRequestContextGetter();
  // The first request context is always a normal (non-OTR) request context.
  // Even when Chromium is started in OTR mode, a normal profile is always
  // created first.
  if (!default_request_context_) {
    default_request_context_ = request_context;
    request_context->set_is_main(true);
    // TODO(eroman): this isn't terribly useful anymore now that the
    // net::URLRequestContext is constructed by the IO thread...
    NotificationService::current()->Notify(
        NotificationType::DEFAULT_REQUEST_CONTEXT_AVAILABLE,
        NotificationService::AllSources(), NotificationService::NoDetails());
  }

  return request_context;
}

URLRequestContextGetter* ProfileImpl::GetRequestContextForMedia() {
  return io_data_.GetMediaRequestContextGetter();
}

FaviconService* ProfileImpl::GetFaviconService(ServiceAccessType sat) {
  if (!favicon_service_created_) {
    favicon_service_created_ = true;
    scoped_refptr<FaviconService> service(new FaviconService(this));
    favicon_service_.swap(service);
  }
  return favicon_service_.get();
}

URLRequestContextGetter* ProfileImpl::GetRequestContextForExtensions() {
  return io_data_.GetExtensionsRequestContextGetter();
}

void ProfileImpl::RegisterExtensionWithRequestContexts(
    const Extension* extension) {
  // AddRef to ensure the data lives until the other thread gets it. Balanced in
  // OnNewExtensions.
  extension->AddRef();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(extension_info_map_.get(),
                        &ExtensionInfoMap::AddExtension,
                        extension));
}

void ProfileImpl::UnregisterExtensionWithRequestContexts(
    const Extension* extension) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(extension_info_map_.get(),
                        &ExtensionInfoMap::RemoveExtension,
                        extension->id()));
}

net::SSLConfigService* ProfileImpl::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

HostContentSettingsMap* ProfileImpl::GetHostContentSettingsMap() {
  if (!host_content_settings_map_.get())
    host_content_settings_map_ = new HostContentSettingsMap(this);
  return host_content_settings_map_.get();
}

HostZoomMap* ProfileImpl::GetHostZoomMap() {
  if (!host_zoom_map_)
    host_zoom_map_ = new HostZoomMap(this);
  return host_zoom_map_.get();
}

GeolocationContentSettingsMap* ProfileImpl::GetGeolocationContentSettingsMap() {
  if (!geolocation_content_settings_map_.get())
    geolocation_content_settings_map_ = new GeolocationContentSettingsMap(this);
  return geolocation_content_settings_map_.get();
}

GeolocationPermissionContext* ProfileImpl::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_.get())
    geolocation_permission_context_ = new GeolocationPermissionContext(this);
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

    // Send out the notification that the history service was created.
    NotificationService::current()->
        Notify(NotificationType::HISTORY_CREATED, Source<Profile>(this),
               Details<HistoryService>(history_service_.get()));
  }
  return history_service_.get();
}

HistoryService* ProfileImpl::GetHistoryServiceWithoutCreating() {
  return history_service_.get();
}

TemplateURLModel* ProfileImpl::GetTemplateURLModel() {
  if (!template_url_model_.get())
    template_url_model_.reset(new TemplateURLModel(this));
  return template_url_model_.get();
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
  ps = new PasswordStoreMac(new MacKeychain(), login_db);
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
  } else {  // Detect the store to use automatically.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    desktop_env = base::nix::GetDesktopEnvironment(env.get());
    VLOG(1) << "Password storage detected desktop environment: "
            << base::nix::GetDesktopEnvironmentName(desktop_env);
  }

  scoped_ptr<PasswordStoreX::NativeBackend> backend;
  if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE4) {
    // KDE3 didn't use DBus, which our KWallet store uses.
    VLOG(1) << "Trying KWallet for password storage.";
    backend.reset(new NativeBackendKWallet());
    if (backend->Init())
      VLOG(1) << "Using KWallet for password storage.";
    else
      backend.reset();
  } else if (desktop_env == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
             desktop_env == base::nix::DESKTOP_ENVIRONMENT_XFCE) {
#if defined(USE_GNOME_KEYRING)
    VLOG(1) << "Trying GNOME keyring for password storage.";
    backend.reset(new NativeBackendGnome());
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
    scoped_refptr<DownloadManager> dlm(
        new DownloadManager(g_browser_process->download_status_updater()));
    dlm->Init(this);
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

fileapi::SandboxedFileSystemContext* ProfileImpl::GetFileSystemContext() {
  if (!file_system_context_.get())
    file_system_context_ = CreateFileSystemContext(
        GetPath(), IsOffTheRecord());
  DCHECK(file_system_context_.get());
  return file_system_context_.get();
}

void ProfileImpl::InitThemes() {
  if (!created_theme_provider_) {
#if defined(TOOLKIT_USES_GTK)
    theme_provider_.reset(new GtkThemeProvider);
#else
    theme_provider_.reset(new BrowserThemeProvider);
#endif
    theme_provider_->Init(this);
    created_theme_provider_ = true;
  }
}

void ProfileImpl::SetTheme(const Extension* extension) {
  InitThemes();
  theme_provider_.get()->SetTheme(extension);
}

void ProfileImpl::SetNativeTheme() {
  InitThemes();
  theme_provider_.get()->SetNativeTheme();
}

void ProfileImpl::ClearTheme() {
  InitThemes();
  theme_provider_.get()->UseDefaultTheme();
}

const Extension* ProfileImpl::GetTheme() {
  InitThemes();

  std::string id = theme_provider_.get()->GetThemeID();
  if (id == BrowserThemeProvider::kDefaultThemeID)
    return NULL;

  return extensions_service_->GetExtensionById(id, false);
}

BrowserThemeProvider* ProfileImpl::GetThemeProvider() {
  InitThemes();
  return theme_provider_.get();
}

SessionService* ProfileImpl::GetSessionService() {
  if (!session_service_.get() && !shutdown_session_service_) {
    session_service_ = new SessionService(this);
    session_service_->ResetFromCurrentBrowsers();
  }
  return session_service_.get();
}

void ProfileImpl::ShutdownSessionService() {
  if (shutdown_session_service_)
    return;

  // We're about to exit, force creation of the session service if it hasn't
  // been created yet. We do this to ensure session state matches the point in
  // time the user exited.
  GetSessionService();
  shutdown_session_service_ = true;
  session_service_ = NULL;
}

bool ProfileImpl::HasSessionService() const {
  return (session_service_.get() != NULL);
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

bool ProfileImpl::IsSameProfile(Profile* profile) {
  if (profile == static_cast<Profile*>(this))
    return true;
  Profile* otr_profile = off_the_record_profile_.get();
  return otr_profile && profile == otr_profile;
}

Time ProfileImpl::GetStartTime() const {
  return start_time_;
}

TabRestoreService* ProfileImpl::GetTabRestoreService() {
  if (!tab_restore_service_.get())
    tab_restore_service_ = new TabRestoreService(this);
  return tab_restore_service_.get();
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

void ProfileImpl::ResetTabRestoreService() {
  tab_restore_service_ = NULL;
}

SpellCheckHost* ProfileImpl::GetSpellCheckHost() {
  return spellcheck_host_ready_ ? spellcheck_host_.get() : NULL;
}

void ProfileImpl::ReinitializeSpellCheckHost(bool force) {
  // If we are already loading the spellchecker, and this is just a hint to
  // load the spellchecker, do nothing.
  if (!force && spellcheck_host_.get())
    return;

  spellcheck_host_ready_ = false;

  bool notify = false;
  if (spellcheck_host_.get()) {
    spellcheck_host_->UnsetObserver();
    spellcheck_host_ = NULL;
    notify = true;
  }

  PrefService* prefs = GetPrefs();
  if (prefs->GetBoolean(prefs::kEnableSpellCheck)) {
    // Retrieve the (perhaps updated recently) dictionary name from preferences.
    spellcheck_host_ = new SpellCheckHost(
        this,
        prefs->GetString(prefs::kSpellCheckDictionary),
        GetRequestContext());
    spellcheck_host_->Initialize();
  } else if (notify) {
    // The spellchecker has been disabled.
    SpellCheckHostInitialized();
  }
}

void ProfileImpl::SpellCheckHostInitialized() {
  spellcheck_host_ready_ = spellcheck_host_ &&
      (spellcheck_host_->bdict_file() != base::kInvalidPlatformFileValue ||
       spellcheck_host_->use_platform_spellchecker());
  NotificationService::current()->Notify(
      NotificationType::SPELLCHECK_HOST_REINITIALIZED,
          Source<Profile>(this), NotificationService::NoDetails());
}

ExtensionPrefValueMap* ProfileImpl::GetExtensionPrefValueMap() {
  if (!extension_pref_value_map_.get())
    extension_pref_value_map_.reset(new ExtensionPrefValueMap);
  return extension_pref_value_map_.get();
}

WebKitContext* ProfileImpl::GetWebKitContext() {
  if (!webkit_context_.get())
    webkit_context_ = new WebKitContext(this, clear_local_state_on_exit_);
  DCHECK(webkit_context_.get());
  return webkit_context_.get();
}

DesktopNotificationService* ProfileImpl::GetDesktopNotificationService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!desktop_notification_service_.get()) {
     desktop_notification_service_.reset(new DesktopNotificationService(
         this, g_browser_process->notification_ui_manager()));
  }
  return desktop_notification_service_.get();
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

void ProfileImpl::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (NotificationType::PREF_CHANGED == type) {
    std::string* pref_name_in = Details<std::string>(details).ptr();
    PrefService* prefs = Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kSpellCheckDictionary ||
        *pref_name_in == prefs::kEnableSpellCheck) {
      ReinitializeSpellCheckHost(true);
    } else if (*pref_name_in == prefs::kEnableAutoSpellCorrect) {
      NotificationService::current()->Notify(
          NotificationType::SPELLCHECK_AUTOSPELL_TOGGLED,
              Source<Profile>(this), NotificationService::NoDetails());
    } else if (*pref_name_in == prefs::kClearSiteDataOnExit) {
      clear_local_state_on_exit_ =
          prefs->GetBoolean(prefs::kClearSiteDataOnExit);
      if (webkit_context_) {
        webkit_context_->set_clear_local_state_on_exit(
            clear_local_state_on_exit_);
      }
      if (appcache_service_) {
        appcache_service_->SetClearLocalStateOnExit(
            clear_local_state_on_exit_);
      }
    }
  } else if (NotificationType::THEME_INSTALLED == type) {
    DCHECK_EQ(Source<Profile>(source).ptr(), GetOriginalProfile());
    const Extension* extension = Details<const Extension>(details).ptr();
    SetTheme(extension);
  } else if (NotificationType::BOOKMARK_MODEL_LOADED == type) {
    GetProfileSyncService();  // Causes lazy-load if sync is enabled.
    registrar_.Remove(this, NotificationType::BOOKMARK_MODEL_LOADED,
                      Source<Profile>(this));
  }
}

void ProfileImpl::StopCreateSessionServiceTimer() {
  create_session_service_timer_.Stop();
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

BrowserSignin* ProfileImpl::GetBrowserSignin() {
  if (!browser_signin_.get()) {
    browser_signin_.reset(new BrowserSignin(this));
  }
  return browser_signin_.get();
}

CloudPrintProxyService* ProfileImpl::GetCloudPrintProxyService() {
  if (!cloud_print_proxy_service_.get())
    InitCloudPrintProxyService();
  return cloud_print_proxy_service_.get();
}

void ProfileImpl::InitSyncService(const std::string& cros_user) {
  profile_sync_factory_.reset(
      new ProfileSyncFactoryImpl(this, CommandLine::ForCurrentProcess()));
  sync_service_.reset(
      profile_sync_factory_->CreateProfileSyncService(cros_user));
  sync_service_->Initialize();
}

void ProfileImpl::InitCloudPrintProxyService() {
  cloud_print_proxy_service_ = new CloudPrintProxyService(this);
  cloud_print_proxy_service_->Initialize();
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

policy::ProfilePolicyContext* ProfileImpl::GetPolicyContext() {
  if (!profile_policy_context_.get())
    profile_policy_context_.reset(new policy::ProfilePolicyContext(this));

  return profile_policy_context_.get();
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
          GetPrefs()->SetString(prefs::kApplicationLocaleBackup,
                                g_browser_process->GetApplicationLocale());
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
  if (!chromeos::OwnershipService::GetSharedInstance()->IsAlreadyOwned() ||
      chromeos::OwnershipService::GetSharedInstance()->CurrentUserIsOwner()) {
    g_browser_process->local_state()->SetString(
        prefs::kOwnerLocale, new_locale);
    g_browser_process->local_state()->SetString(
        prefs::kApplicationLocale, new_locale);
  }

  GetPrefs()->ScheduleSavePersistentPrefs();
  g_browser_process->local_state()->ScheduleSavePersistentPrefs();
}

chromeos::ProxyConfigServiceImpl*
    ProfileImpl::GetChromeOSProxyConfigServiceImpl() {
  if (!chromeos_proxy_config_service_impl_) {
    chromeos_proxy_config_service_impl_ =
        new chromeos::ProxyConfigServiceImpl();
  }
  return chromeos_proxy_config_service_impl_;
}

void ProfileImpl::SetupChromeOSEnterpriseExtensionObserver() {
  DCHECK(!chromeos_enterprise_extension_observer_.get());
  chromeos_enterprise_extension_observer_.reset(
      new chromeos::EnterpriseExtensionObserver(this));
}
#endif  // defined(OS_CHROMEOS)

PrefProxyConfigTracker* ProfileImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_)
    pref_proxy_config_tracker_ = new PrefProxyConfigTracker(GetPrefs());

  return pref_proxy_config_tracker_;
}

PrerenderManager* ProfileImpl::GetPrerenderManager() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(switches::kEnablePagePrerender))
    return NULL;
  if (!prerender_manager_)
    prerender_manager_ = new PrerenderManager(this);
  return prerender_manager_;
}
