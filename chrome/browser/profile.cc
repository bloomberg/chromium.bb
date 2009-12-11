// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile.h"

#include "app/theme_provider.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/ntp_resource_cache.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/spellcheck_host.h"
#include "chrome/browser/strict_transport_security_persister.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/browser/privacy_blacklist/blacklist_manager.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/search_versus_navigate_classifier.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/ssl/ssl_host_state.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/thumbnail_store.h"
#include "chrome/browser/visitedlink_master.h"
#include "chrome/browser/visitedlink_event_listener.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/web_resource/web_resource_service.h"
#include "chrome/common/appcache/chrome_appcache_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "grit/locale_settings.h"
#include "net/base/strict_transport_security_state.h"
#include "webkit/database/database_tracker.h"

#if defined(OS_LINUX)
#include "chrome/browser/gtk/gtk_theme_provider.h"
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
  std::wstring user_path(CommandLine::ForCurrentProcess()->GetSwitchValue(
                             switches::kDiskCacheDir));

  if (!user_path.empty()) {
    *cache_path = FilePath::FromWStringHack(user_path);
  }

  const char* arg = kNormalContext == type ? switches::kDiskCacheSize :
                                             switches::kMediaCacheSize;
  std::string value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(arg);

  // By default we let the cache determine the right size.
  *max_size = 0;
  if (!StringToInt(value, max_size)) {
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

bool HasACacheSubdir(const FilePath &dir) {
  return file_util::PathExists(GetCachePath(dir)) ||
         file_util::PathExists(GetMediaCachePath(dir));
}

}  // namespace

// A pointer to the request context for the default profile.  See comments on
// Profile::GetDefaultRequestContext.
URLRequestContextGetter* Profile::default_request_context_;

static void CleanupRequestContext(ChromeURLRequestContextGetter* context) {
  if (context)
    context->CleanupOnUIThread();
}

// static
const ProfileId Profile::InvalidProfileId = static_cast<ProfileId>(0);

// static
void Profile::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kSearchSuggestEnabled, true);
  prefs->RegisterBooleanPref(prefs::kSessionExitedCleanly, true);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, true);
  // TODO(estade): IDS_SPELLCHECK_DICTIONARY should be an ASCII string.
  prefs->RegisterLocalizedStringPref(prefs::kSpellCheckDictionary,
      IDS_SPELLCHECK_DICTIONARY);
  prefs->RegisterBooleanPref(prefs::kEnableSpellCheck, true);
  prefs->RegisterBooleanPref(prefs::kEnableAutoSpellCorrect, true);
#if defined(OS_LINUX)
  prefs->RegisterBooleanPref(prefs::kUsesSystemTheme, false);
#endif
  prefs->RegisterFilePathPref(prefs::kCurrentThemePackFilename, FilePath());
  prefs->RegisterStringPref(prefs::kCurrentThemeID,
                            UTF8ToWide(BrowserThemeProvider::kDefaultThemeID));
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeImages);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeColors);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeTints);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeDisplayProperties);
  prefs->RegisterBooleanPref(prefs::kDisableExtensions, false);
}

// static
Profile* Profile::CreateProfile(const FilePath& path) {
  return new ProfileImpl(path);
}

// static
URLRequestContextGetter* Profile::GetDefaultRequestContext() {
  return default_request_context_;
}

#if defined(OS_LINUX)
// Temporarily disabled while we figure some stuff out.
// http://code.google.com/p/chromium/issues/detail?id=12351
// #include "chrome/browser/password_manager/password_store_gnome.h"
// #include "chrome/browser/password_manager/password_store_kwallet.h"
#elif defined(OS_WIN)
#include "chrome/browser/password_manager/password_store_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/keychain_mac.h"
#include "chrome/browser/password_manager/login_database_mac.h"
#include "chrome/browser/password_manager/password_store_mac.h"
#endif

////////////////////////////////////////////////////////////////////////////////
//
// OffTheRecordProfileImpl is a profile subclass that wraps an existing profile
// to make it suitable for the off the record mode.
//
////////////////////////////////////////////////////////////////////////////////
class OffTheRecordProfileImpl : public Profile,
                                public NotificationObserver {
 public:
  explicit OffTheRecordProfileImpl(Profile* real_profile)
      : profile_(real_profile),
        extensions_request_context_(NULL),
        start_time_(Time::Now()) {
    request_context_ = ChromeURLRequestContextGetter::CreateOffTheRecord(this);

    // Register for browser close notifications so we can detect when the last
    // off-the-record window is closed, in which case we can clean our states
    // (cookies, downloads...).
    registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                   NotificationService::AllSources());
  }

  virtual ~OffTheRecordProfileImpl() {
    CleanupRequestContext(request_context_);
    CleanupRequestContext(extensions_request_context_);
  }

  virtual ProfileId GetRuntimeId() {
    return reinterpret_cast<ProfileId>(this);
  }

  virtual FilePath GetPath() { return profile_->GetPath(); }

  virtual bool IsOffTheRecord() {
    return true;
  }

  virtual Profile* GetOffTheRecordProfile() {
    return this;
  }

  virtual void DestroyOffTheRecordProfile() {
    // Suicide is bad!
    NOTREACHED();
  }

  virtual Profile* GetOriginalProfile() {
    return profile_;
  }

  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() {
    if (!db_tracker_)
      db_tracker_ = new webkit_database::DatabaseTracker(FilePath());
    return db_tracker_;
  }

  virtual VisitedLinkMaster* GetVisitedLinkMaster() {
    // We don't provide access to the VisitedLinkMaster when we're OffTheRecord
    // because we don't want to leak the sites that the user has visited before.
    return NULL;
  }

  virtual ExtensionsService* GetExtensionsService() {
    return NULL;
  }

  virtual UserScriptMaster* GetUserScriptMaster() {
    return profile_->GetUserScriptMaster();
  }

  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager() {
    return NULL;
  }

  virtual ExtensionProcessManager* GetExtensionProcessManager() {
    return NULL;
  }

  virtual ExtensionMessageService* GetExtensionMessageService() {
    return NULL;
  }

  virtual SSLHostState* GetSSLHostState() {
    if (!ssl_host_state_.get())
      ssl_host_state_.reset(new SSLHostState());

    DCHECK(ssl_host_state_->CalledOnValidThread());
    return ssl_host_state_.get();
  }

  virtual net::StrictTransportSecurityState* GetStrictTransportSecurityState() {
    if (!strict_transport_security_state_.get()) {
      strict_transport_security_state_ =
          new net::StrictTransportSecurityState();
    }

    return strict_transport_security_state_.get();
  }

  virtual HistoryService* GetHistoryService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS) {
      return profile_->GetHistoryService(sat);
    } else {
      NOTREACHED() << "This profile is OffTheRecord";
      return NULL;
    }
  }

  virtual HistoryService* GetHistoryServiceWithoutCreating() {
    return profile_->GetHistoryServiceWithoutCreating();
  }

  virtual FaviconService* GetFaviconService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS) {
      return profile_->GetFaviconService(sat);
    } else {
      NOTREACHED() << "This profile is OffTheRecord";
      return NULL;
    }
  }

  virtual SearchVersusNavigateClassifier* GetSearchVersusNavigateClassifier() {
    return profile_->GetSearchVersusNavigateClassifier();
  }

  virtual WebDataService* GetWebDataService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS) {
      return profile_->GetWebDataService(sat);
    } else {
      NOTREACHED() << "This profile is OffTheRecord";
      return NULL;
    }
  }

  virtual WebDataService* GetWebDataServiceWithoutCreating() {
    return profile_->GetWebDataServiceWithoutCreating();
  }

  virtual PasswordStore* GetPasswordStore(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS) {
      return profile_->GetPasswordStore(sat);
    } else {
      NOTREACHED() << "This profile is OffTheRecord";
      return NULL;
    }
  }

  virtual PrefService* GetPrefs() {
    return profile_->GetPrefs();
  }

  virtual TemplateURLModel* GetTemplateURLModel() {
    return profile_->GetTemplateURLModel();
  }

  virtual TemplateURLFetcher* GetTemplateURLFetcher() {
    return profile_->GetTemplateURLFetcher();
  }

  virtual DownloadManager* GetDownloadManager() {
    if (!download_manager_.get()) {
      scoped_refptr<DownloadManager> dlm(new DownloadManager);
      dlm->Init(this);
      download_manager_.swap(dlm);
    }
    return download_manager_.get();
  }

  virtual bool HasCreatedDownloadManager() const {
    return (download_manager_.get() != NULL);
  }

  virtual PersonalDataManager* GetPersonalDataManager() {
    return NULL;
  }

  virtual void InitThemes() {
    GetOriginalProfile()->InitThemes();
  }

  virtual void SetTheme(Extension* extension) {
    GetOriginalProfile()->SetTheme(extension);
  }

  virtual void SetNativeTheme() {
    GetOriginalProfile()->SetNativeTheme();
  }

  virtual void ClearTheme() {
    GetOriginalProfile()->ClearTheme();
  }

  virtual Extension* GetTheme() {
    return GetOriginalProfile()->GetTheme();
  }

  virtual ThemeProvider* GetThemeProvider() {
    return GetOriginalProfile()->GetThemeProvider();
  }

  virtual URLRequestContextGetter* GetRequestContext() {
    return request_context_;
  }

  virtual URLRequestContextGetter* GetRequestContextForMedia() {
    // In OTR mode, media request context is the same as the original one.
    return request_context_;
  }

  URLRequestContextGetter* GetRequestContextForExtensions() {
    if (!extensions_request_context_) {
      extensions_request_context_ =
          ChromeURLRequestContextGetter::CreateOffTheRecordForExtensions(this);
    }

    return extensions_request_context_;
  }

  virtual net::SSLConfigService* GetSSLConfigService() {
    return GetOriginalProfile()->GetSSLConfigService();
  }

  virtual HostZoomMap* GetHostZoomMap() {
    // Need to use a separate map from the normal one to avoid persisting zoom
    // changes in OTR mode.
    if (!host_zoom_map_)
      host_zoom_map_ = new HostZoomMap(this);
    return host_zoom_map_.get();
  }

  virtual BlacklistManager* GetBlacklistManager() {
    return GetOriginalProfile()->GetBlacklistManager();
  }

  virtual SessionService* GetSessionService() {
    // Don't save any sessions when off the record.
    return NULL;
  }

  virtual void ShutdownSessionService() {
    // We don't allow a session service, nothing to do.
  }

  virtual bool HasSessionService() const {
    // We never have a session service.
    return false;
  }

  virtual bool DidLastSessionExitCleanly() {
    return profile_->DidLastSessionExitCleanly();
  }

  virtual BookmarkModel* GetBookmarkModel() {
    return profile_->GetBookmarkModel();
  }

  virtual DesktopNotificationService* GetDesktopNotificationService() {
    if (!desktop_notification_service_.get()) {
      desktop_notification_service_.reset(new DesktopNotificationService(
          this, g_browser_process->notification_ui_manager()));
    }
    return desktop_notification_service_.get();
  }

  virtual ProfileSyncService* GetProfileSyncService() {
    return NULL;
  }

  virtual bool IsSameProfile(Profile* profile) {
    if (profile == static_cast<Profile*>(this))
      return true;
    return profile == profile_;
  }

  virtual Time GetStartTime() const {
    return start_time_;
  }

  virtual TabRestoreService* GetTabRestoreService() {
    return NULL;
  }

  virtual void ResetTabRestoreService() {
  }

  virtual SpellCheckHost* GetSpellCheckHost() {
    return profile_->GetSpellCheckHost();
  }

  virtual void ReinitializeSpellCheckHost(bool force) {
    profile_->ReinitializeSpellCheckHost(force);
  }

  virtual WebKitContext* GetWebKitContext() {
    if (!webkit_context_.get())
      webkit_context_ = new WebKitContext(FilePath(), true);
    DCHECK(webkit_context_.get());
    return webkit_context_.get();
  }

  virtual ThumbnailStore* GetThumbnailStore() {
    return NULL;
  }

  virtual void MarkAsCleanShutdown() {
  }

  virtual void InitExtensions() {
    NOTREACHED();
  }

  virtual void InitWebResources() {
    NOTREACHED();
  }

  virtual NTPResourceCache* GetNTPResourceCache() {
    // Just return the real profile resource cache.
    return profile_->GetNTPResourceCache();
  }

  virtual void ExitedOffTheRecordMode() {
    // Drop our download manager so we forget about all the downloads made
    // in off-the-record mode.
    download_manager_ = NULL;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK_EQ(NotificationType::BROWSER_CLOSED, type.value);
    // We are only interested in OTR browser closing.
    if (Source<Browser>(source)->profile() != this)
      return;

    // Let's check if we still have an Off The Record window opened.
    // Note that we check against 1 as this notification is sent before the
    // browser window is actually removed from the list.
    if (BrowserList::GetBrowserCount(this) <= 1)
      ExitedOffTheRecordMode();
  }

 private:
  NotificationRegistrar registrar_;

  // The real underlying profile.
  Profile* profile_;

  // The context to use for requests made from this OTR session.
  scoped_refptr<ChromeURLRequestContextGetter> request_context_;

  scoped_refptr<ChromeURLRequestContextGetter> extensions_request_context_;

  scoped_refptr<HostZoomMap> host_zoom_map_;

  // The download manager that only stores downloaded items in memory.
  scoped_refptr<DownloadManager> download_manager_;

  // Use a separate desktop notification service for OTR.
  scoped_ptr<DesktopNotificationService> desktop_notification_service_;

  // Use a special WebKit context for OTR browsing.
  scoped_refptr<WebKitContext> webkit_context_;

  // We don't want SSLHostState from the OTR profile to leak back to the main
  // profile because then the main profile would learn some of the host names
  // the user visited while OTR.
  scoped_ptr<SSLHostState> ssl_host_state_;

  // The StrictTransportSecurityState that only stores enabled sites in memory.
  scoped_refptr<net::StrictTransportSecurityState>
      strict_transport_security_state_;

  // Time we were started.
  Time start_time_;

  // The main database tracker for this profile.
  // Should be used only on the file thread.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordProfileImpl);
};

ProfileImpl::ProfileImpl(const FilePath& path)
    : path_(path),
      visited_link_event_listener_(new VisitedLinkEventListener()),
      extension_devtools_manager_(NULL),
      request_context_(NULL),
      media_request_context_(NULL),
      extensions_request_context_(NULL),
      host_zoom_map_(NULL),
      blacklist_manager_(NULL),
      blacklist_manager_created_(false),
      history_service_created_(false),
      favicon_service_created_(false),
      created_web_data_service_(false),
      created_password_store_(false),
      created_download_manager_(false),
      created_theme_provider_(false),
      start_time_(Time::Now()),
      spellcheck_host_(NULL),
      spellcheck_host_ready_(false),
      shutdown_session_service_(false) {
  DCHECK(!path.empty()) << "Using an empty path will attempt to write " <<
                            "profile files to the root directory!";
  create_session_service_timer_.Start(
      TimeDelta::FromMilliseconds(kCreateSessionServiceDelayMS), this,
      &ProfileImpl::EnsureSessionServiceCreated);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExtensionTimelineApi)) {
    extension_devtools_manager_ = new ExtensionDevToolsManager(this);
  }

  extension_process_manager_.reset(new ExtensionProcessManager(this));
  extension_message_service_ = new ExtensionMessageService(this);

  PrefService* prefs = GetPrefs();
  prefs->AddPrefObserver(prefs::kSpellCheckDictionary, this);
  prefs->AddPrefObserver(prefs::kEnableSpellCheck, this);
  prefs->AddPrefObserver(prefs::kEnableAutoSpellCorrect, this);

#if defined(OS_MACOSX)
  // If the profile directory doesn't already have a cache directory and it
  // is under ~/Library/Application Support, use a suitable cache directory
  // under ~/Library/Caches.  For example, a profile directory of
  // ~/Library/Application Support/Google/Chrome/MyProfileName that doesn't
  // have a "Cache" or "MediaCache" subdirectory would use the cache directory
  // ~/Library/Caches/Google/Chrome/MyProfileName.
  //
  // TODO(akalin): Come up with unit tests for this.
  if (!HasACacheSubdir(path_)) {
    FilePath app_data_path, user_cache_path;
    if (PathService::Get(base::DIR_APP_DATA, &app_data_path) &&
        PathService::Get(base::DIR_USER_CACHE, &user_cache_path) &&
        app_data_path.AppendRelativePath(path_, &user_cache_path)) {
      base_cache_path_ = user_cache_path;
    }
  }
#elif defined(OS_POSIX)  // Posix minus Mac.
  // See http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
  // for a spec on where cache files go.  The net effect for most systems is we
  // use ~/.cache/chromium/ for Chromium and ~/.cache/google-chrome/ for
  // official builds.
  if (!PathService::IsOverridden(chrome::DIR_USER_DATA)) {
#if defined(GOOGLE_CHROME_BUILD)
    const char kCacheDir[] = "google-chrome";
#else
    const char kCacheDir[] = "chromium";
#endif
    PathService::Get(base::DIR_USER_CACHE, &base_cache_path_);
    base_cache_path_ = base_cache_path_.Append(kCacheDir);
    if (!file_util::PathExists(base_cache_path_))
      file_util::CreateDirectory(base_cache_path_);
  }
#endif
  if (base_cache_path_.empty())
    base_cache_path_ = path_;

  // Listen for theme installation.
  registrar_.Add(this, NotificationType::THEME_INSTALLED,
                 NotificationService::AllSources());

  // Listen for bookmark model load, to bootstrap the sync service.
  registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                 Source<Profile>(this));

  ssl_config_service_manager_.reset(
      SSLConfigServiceManager::CreateDefaultManager(this));

#if defined(OS_CHROMEOS)
  chromeos_preferences_.Init(prefs);
#endif
}

void ProfileImpl::InitExtensions() {
  if (user_script_master_ || extensions_service_)
    return;  // Already initialized.

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  ExtensionErrorReporter::Init(true);  // allow noisy errors.

  FilePath script_dir;  // Don't look for user scripts in any directory.
                        // TODO(aa): We should just remove this functionality,
                        // since it isn't used anymore.
  user_script_master_ = new UserScriptMaster(script_dir);

  extensions_service_ = new ExtensionsService(
      this,
      CommandLine::ForCurrentProcess(),
      GetPrefs(),
      GetPath().AppendASCII(ExtensionsService::kInstallDirectoryName),
      true);

  extensions_service_->Init();

  // Load any extensions specified with --load-extension.
  if (command_line->HasSwitch(switches::kLoadExtension)) {
    std::wstring path_string =
        command_line->GetSwitchValue(switches::kLoadExtension);
    FilePath path = FilePath::FromWStringHack(path_string);
    extensions_service_->LoadExtension(path);

    // Tell UserScriptMaser to watch this extension's directory for changes so
    // you can live edit content scripts during development.
    user_script_master_->AddWatchedPath(path);
  }
}

void ProfileImpl::InitWebResources() {
  if (web_resource_service_)
    return;  // Already initialized.

  web_resource_service_ = new WebResourceService(this);
  web_resource_service_->StartAfterDelay();
}

NTPResourceCache* ProfileImpl::GetNTPResourceCache() {
  if (!ntp_resource_cache_.get())
    ntp_resource_cache_.reset(new NTPResourceCache(this));
  return ntp_resource_cache_.get();
}

ProfileImpl::~ProfileImpl() {
  tab_restore_service_ = NULL;

  StopCreateSessionServiceTimer();
  // TemplateURLModel schedules a task on the WebDataService from its
  // destructor. Delete it first to ensure the task gets scheduled before we
  // shut down the database.
  template_url_model_.reset();

  // The download manager queries the history system and should be deleted
  // before the history is shutdown so it can properly cancel all requests.
  download_manager_ = NULL;

  // The theme provider provides bitmaps to whoever wants them.
  theme_provider_.reset();

  // The ThumbnailStore saves thumbnails used by the NTP.  Call Shutdown to
  // save any new thumbnails to disk and release its reference to the
  // HistoryService.
  if (thumbnail_store_.get())
    thumbnail_store_->Shutdown();

  // Remove pref observers.
  PrefService* prefs = GetPrefs();
  prefs->RemovePrefObserver(prefs::kSpellCheckDictionary, this);
  prefs->RemovePrefObserver(prefs::kEnableSpellCheck, this);
  prefs->RemovePrefObserver(prefs::kEnableAutoSpellCorrect, this);

  // Delete the NTP resource cache so we can unregister pref observers.
  ntp_resource_cache_.reset();

  sync_service_.reset();

  // Both HistoryService and WebDataService maintain threads for background
  // processing. Its possible each thread still has tasks on it that have
  // increased the ref count of the service. In such a situation, when we
  // decrement the refcount, it won't be 0, and the threads/databases aren't
  // properly shut down. By explicitly calling Cleanup/Shutdown we ensure the
  // databases are properly closed.
  if (web_data_service_.get())
    web_data_service_->Shutdown();

  if (history_service_.get())
    history_service_->Cleanup();

  if (spellcheck_host_.get())
    spellcheck_host_->UnsetObserver();

  if (default_request_context_ == request_context_)
    default_request_context_ = NULL;

  CleanupRequestContext(request_context_);
  CleanupRequestContext(media_request_context_);
  CleanupRequestContext(extensions_request_context_);

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

  extension_message_service_->ProfileDestroyed();

  if (extensions_service_)
    extensions_service_->ProfileDestroyed();

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
    scoped_ptr<OffTheRecordProfileImpl> p(new OffTheRecordProfileImpl(this));
    off_the_record_profile_.swap(p);
  }
  return off_the_record_profile_.get();
}

void ProfileImpl::DestroyOffTheRecordProfile() {
  off_the_record_profile_.reset();
}

Profile* ProfileImpl::GetOriginalProfile() {
  return this;
}

webkit_database::DatabaseTracker* ProfileImpl::GetDatabaseTracker() {
  if (!db_tracker_)
    db_tracker_ = new webkit_database::DatabaseTracker(GetPath());
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

ExtensionsService* ProfileImpl::GetExtensionsService() {
  return extensions_service_.get();
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

SSLHostState* ProfileImpl::GetSSLHostState() {
  if (!ssl_host_state_.get())
    ssl_host_state_.reset(new SSLHostState());

  DCHECK(ssl_host_state_->CalledOnValidThread());
  return ssl_host_state_.get();
}

net::StrictTransportSecurityState*
    ProfileImpl::GetStrictTransportSecurityState() {
  if (!strict_transport_security_state_.get()) {
    strict_transport_security_state_ = new net::StrictTransportSecurityState();
    strict_transport_security_persister_ =
        new StrictTransportSecurityPersister();
    strict_transport_security_persister_->Initialize(
        strict_transport_security_state_.get(), path_);
  }

  return strict_transport_security_state_.get();
}

PrefService* ProfileImpl::GetPrefs() {
  if (!prefs_.get()) {
    prefs_.reset(new PrefService(GetPrefFilePath()));

    // The Profile class and ProfileManager class may read some prefs so
    // register known prefs as soon as possible.
    Profile::RegisterUserPrefs(prefs_.get());
    browser::RegisterUserPrefs(prefs_.get());

    // The last session exited cleanly if there is no pref for
    // kSessionExitedCleanly or the value for kSessionExitedCleanly is true.
    last_session_exited_cleanly_ =
        prefs_->GetBoolean(prefs::kSessionExitedCleanly);
    // Mark the session as open.
    prefs_->SetBoolean(prefs::kSessionExitedCleanly, false);
    // Make sure we save to disk that the session has opened.
    prefs_->ScheduleSavePersistentPrefs();
  }

  return prefs_.get();
}

FilePath ProfileImpl::GetPrefFilePath() {
  FilePath pref_file_path = path_;
  pref_file_path = pref_file_path.Append(chrome::kPreferencesFilename);
  return pref_file_path;
}

URLRequestContextGetter* ProfileImpl::GetRequestContext() {
  if (!request_context_) {
    FilePath cookie_path = GetPath();
    cookie_path = cookie_path.Append(chrome::kCookieFilename);
    FilePath cache_path = base_cache_path_;
    int max_size;
    GetCacheParameters(kNormalContext, &cache_path, &max_size);

    cache_path = GetCachePath(cache_path);
    request_context_ = ChromeURLRequestContextGetter::CreateOriginal(
        this, cookie_path, cache_path, max_size);

    // The first request context is always a normal (non-OTR) request context.
    // Even when Chromium is started in OTR mode, a normal profile is always
    // created first.
    if (!default_request_context_) {
      default_request_context_ = request_context_;
      // TODO(eroman): this isn't terribly useful anymore now that the
      // URLRequestContext is constructed by the IO thread...
      NotificationService::current()->Notify(
          NotificationType::DEFAULT_REQUEST_CONTEXT_AVAILABLE,
          NotificationService::AllSources(), NotificationService::NoDetails());
    }
  }

  return request_context_;
}

URLRequestContextGetter* ProfileImpl::GetRequestContextForMedia() {
  if (!media_request_context_) {
    FilePath cache_path = base_cache_path_;
    int max_size;
    GetCacheParameters(kMediaContext, &cache_path, &max_size);

    cache_path = GetMediaCachePath(cache_path);
    media_request_context_ =
        ChromeURLRequestContextGetter::CreateOriginalForMedia(
            this, cache_path, max_size);
  }

  return media_request_context_;
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
  if (!extensions_request_context_) {
    FilePath cookie_path = GetPath();
    cookie_path = cookie_path.Append(chrome::kExtensionsCookieFilename);

    extensions_request_context_ =
        ChromeURLRequestContextGetter::CreateOriginalForExtensions(
            this, cookie_path);
  }

  return extensions_request_context_;
}

net::SSLConfigService* ProfileImpl::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

HostZoomMap* ProfileImpl::GetHostZoomMap() {
  if (!host_zoom_map_)
    host_zoom_map_ = new HostZoomMap(this);
  return host_zoom_map_.get();
}

BlacklistManager* ProfileImpl::GetBlacklistManager() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePrivacyBlacklists)) {
    return NULL;
  }
  if (!blacklist_manager_created_) {
    blacklist_manager_created_ = true;
    blacklist_manager_ = new BlacklistManager(this, GetExtensionsService());
    blacklist_manager_->Initialize();
  }
  return blacklist_manager_.get();
}

HistoryService* ProfileImpl::GetHistoryService(ServiceAccessType sat) {
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

SearchVersusNavigateClassifier*
ProfileImpl::GetSearchVersusNavigateClassifier() {
  if (!search_versus_navigate_classifier_.get()) {
    search_versus_navigate_classifier_.reset(
        new SearchVersusNavigateClassifier(this));
  }
  return search_versus_navigate_classifier_.get();
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
#if defined(OS_LINUX)
  // TODO(evanm): implement "native" password management.
  // This bug describes the issues.
  // http://code.google.com/p/chromium/issues/detail?id=12351
  ps = new PasswordStoreDefault(GetWebDataService(Profile::IMPLICIT_ACCESS));
  if (!ps->Init())
    return;
#elif defined(OS_WIN)
  ps = new PasswordStoreWin(GetWebDataService(Profile::IMPLICIT_ACCESS));
#elif defined(OS_MACOSX)
  FilePath login_db_file_path = GetPath();
  login_db_file_path = login_db_file_path.Append(chrome::kLoginDataFileName);
  LoginDatabaseMac* login_db = new LoginDatabaseMac();
  if (!login_db->Init(login_db_file_path)) {
    LOG(ERROR) << "Could not initialize login database.";
    delete login_db;
    return;
  }
  ps = new PasswordStoreMac(new MacKeychain(), login_db);
#else
  NOTIMPLEMENTED();
#endif
  if (!ps || !ps->Init()) {
    // Try falling back to the default password manager
    LOG(WARNING) << "Could not initialise native password manager - "
                    "falling back to default";
    ps = new PasswordStoreDefault(GetWebDataService(Profile::IMPLICIT_ACCESS));
    if (!ps->Init())
      return;
  }
  password_store_.swap(ps);
}

DownloadManager* ProfileImpl::GetDownloadManager() {
  if (!created_download_manager_) {
    scoped_refptr<DownloadManager> dlm(new DownloadManager);
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
    personal_data_manager_.reset(new PersonalDataManager);
  }
  return personal_data_manager_.get();
}

void ProfileImpl::InitThemes() {
  if (!created_theme_provider_) {
#if defined(OS_LINUX)
    theme_provider_.reset(new GtkThemeProvider);
#else
    theme_provider_.reset(new BrowserThemeProvider);
#endif
    theme_provider_->Init(this);
    created_theme_provider_ = true;
  }
}

void ProfileImpl::SetTheme(Extension* extension) {
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

Extension* ProfileImpl::GetTheme() {
  InitThemes();

  std::string id = theme_provider_.get()->GetThemeID();
  if (id == BrowserThemeProvider::kDefaultThemeID)
    return NULL;

  return extensions_service_->GetExtensionById(id, false);
}

ThemeProvider* ProfileImpl::GetThemeProvider() {
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
  OffTheRecordProfileImpl* otr_profile = off_the_record_profile_.get();
  return otr_profile && profile == static_cast<Profile*>(otr_profile);
}

Time ProfileImpl::GetStartTime() const {
  return start_time_;
}

TabRestoreService* ProfileImpl::GetTabRestoreService() {
  if (!tab_restore_service_.get())
    tab_restore_service_ = new TabRestoreService(this);
  return tab_restore_service_.get();
}

ThumbnailStore* ProfileImpl::GetThumbnailStore() {
  if (!thumbnail_store_.get()) {
    thumbnail_store_ = new ThumbnailStore;
    thumbnail_store_->Init(
        GetPath().Append(chrome::kNewTabThumbnailsFilename), this);
  }
  return thumbnail_store_.get();
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
    spellcheck_host_ = new SpellCheckHost(this,
        WideToASCII(prefs->GetString(prefs::kSpellCheckDictionary)),
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

WebKitContext* ProfileImpl::GetWebKitContext() {
  if (!webkit_context_.get())
    webkit_context_ = new WebKitContext(path_, false);
  DCHECK(webkit_context_.get());
  return webkit_context_.get();
}

DesktopNotificationService* ProfileImpl::GetDesktopNotificationService() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
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
    std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
    PrefService* prefs = Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kSpellCheckDictionary ||
        *pref_name_in == prefs::kEnableSpellCheck) {
      ReinitializeSpellCheckHost(true);
    } else if (*pref_name_in == prefs::kEnableAutoSpellCorrect) {
      NotificationService::current()->Notify(
          NotificationType::SPELLCHECK_AUTOSPELL_TOGGLED,
              Source<Profile>(this), NotificationService::NoDetails());
    }
  } else if (NotificationType::THEME_INSTALLED == type) {
    Extension* extension = Details<Extension>(details).ptr();
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

ProfileSyncService* ProfileImpl::GetProfileSyncService() {
  if (!ProfileSyncService::IsSyncEnabled()) {
    return NULL;
  }
  if (!sync_service_.get())
    InitSyncService();
  return sync_service_.get();
}

void ProfileImpl::InitSyncService() {
  sync_service_.reset(new ProfileSyncService(this));
  sync_service_->Initialize();
}
