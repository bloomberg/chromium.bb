// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/off_the_record_profile_io_data.h"
#include "chrome/browser/ssl/ssl_host_state.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/browser_thread.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "grit/locale_settings.h"
#include "net/base/transport_security_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/database/database_tracker.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_store_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/keychain_mac.h"
#include "chrome/browser/password_manager/password_store_mac.h"
#elif defined(OS_POSIX) && !defined(OS_CHROMEOS)
#include "chrome/browser/password_manager/native_backend_gnome_x.h"
#include "chrome/browser/password_manager/native_backend_kwallet_x.h"
#include "chrome/browser/password_manager/password_store_x.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#endif

using base::Time;
using base::TimeDelta;

// A pointer to the request context for the default profile.  See comments on
// Profile::GetDefaultRequestContext.
URLRequestContextGetter* Profile::default_request_context_;

namespace {

}  // namespace

Profile::Profile()
    : restored_last_session_(false),
      accessibility_pause_level_(0) {
}

// static
const char* Profile::kProfileKey = "__PROFILE__";

// static
const ProfileId Profile::InvalidProfileId = static_cast<ProfileId>(0);

// static
void Profile::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kSearchSuggestEnabled, true);
  prefs->RegisterBooleanPref(prefs::kSessionExitedCleanly, true);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, true);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingReportingEnabled, false);
  // TODO(estade): IDS_SPELLCHECK_DICTIONARY should be an ASCII string.
  prefs->RegisterLocalizedStringPref(prefs::kSpellCheckDictionary,
      IDS_SPELLCHECK_DICTIONARY);
  prefs->RegisterBooleanPref(prefs::kEnableSpellCheck, true);
  prefs->RegisterBooleanPref(prefs::kEnableAutoSpellCorrect, true);
#if defined(TOOLKIT_USES_GTK)
  prefs->RegisterBooleanPref(prefs::kUsesSystemTheme,
                             GtkThemeProvider::DefaultUsesSystemTheme());
#endif
  prefs->RegisterFilePathPref(prefs::kCurrentThemePackFilename, FilePath());
  prefs->RegisterStringPref(prefs::kCurrentThemeID,
                            BrowserThemeProvider::kDefaultThemeID);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeImages);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeColors);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeTints);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeDisplayProperties);
  prefs->RegisterBooleanPref(prefs::kDisableExtensions, false);
  prefs->RegisterStringPref(prefs::kSelectFileLastDirectory, "");
#if defined(OS_CHROMEOS)
  // TODO(dilmah): For OS_CHROMEOS we maintain kApplicationLocale in both
  // local state and user's profile.  For other platforms we maintain
  // kApplicationLocale only in local state.
  // In the future we may want to maintain kApplicationLocale
  // in user's profile for other platforms as well.
  prefs->RegisterStringPref(prefs::kApplicationLocale, "");
  prefs->RegisterStringPref(prefs::kApplicationLocaleBackup, "");
  prefs->RegisterStringPref(prefs::kApplicationLocaleAccepted, "");
#endif
}

// static
URLRequestContextGetter* Profile::GetDefaultRequestContext() {
  return default_request_context_;
}

bool Profile::IsGuestSession() {
#if defined(OS_CHROMEOS)
  static bool is_guest_session =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession);
  return is_guest_session;
#else
  return false;
#endif
}

bool Profile::IsSyncAccessible() {
  ProfileSyncService* syncService = GetProfileSyncService();
  return syncService && !syncService->IsManaged();
}

////////////////////////////////////////////////////////////////////////////////
//
// OffTheRecordProfileImpl is a profile subclass that wraps an existing profile
// to make it suitable for the off the record mode.
//
////////////////////////////////////////////////////////////////////////////////
class OffTheRecordProfileImpl : public Profile,
                                public BrowserList::Observer {
 public:
  explicit OffTheRecordProfileImpl(Profile* real_profile)
      : profile_(real_profile),
        prefs_(real_profile->GetOffTheRecordPrefs()),
        ALLOW_THIS_IN_INITIALIZER_LIST(io_data_(this)),
        start_time_(Time::Now()) {
    extension_process_manager_.reset(ExtensionProcessManager::Create(this));

    BrowserList::AddObserver(this);

    background_contents_service_.reset(
        new BackgroundContentsService(this, CommandLine::ForCurrentProcess()));

    DCHECK(real_profile->GetPrefs()->GetBoolean(prefs::kIncognitoEnabled));

    // TODO(oshima): Remove the need to eagerly initialize the request context
    // getter. chromeos::OnlineAttempt is illegally trying to access this
    // Profile member from a thread other than the UI thread, so we need to
    // prevent a race.
#if defined(OS_CHROMEOS)
    GetRequestContext();
#endif  // defined(OS_CHROMEOS)
  }

  virtual ~OffTheRecordProfileImpl() {
    NotificationService::current()->Notify(NotificationType::PROFILE_DESTROYED,
                                           Source<Profile>(this),
                                           NotificationService::NoDetails());
    // Clean up all DB files/directories
    if (db_tracker_)
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(
              db_tracker_.get(),
              &webkit_database::DatabaseTracker::DeleteIncognitoDBDirectory));

    BrowserList::RemoveObserver(this);

    if (pref_proxy_config_tracker_)
      pref_proxy_config_tracker_->DetachFromPrefService();
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

  virtual bool HasOffTheRecordProfile() {
    return true;
  }

  virtual Profile* GetOriginalProfile() {
    return profile_;
  }

  virtual ChromeAppCacheService* GetAppCacheService() {
    if (!appcache_service_) {
      appcache_service_ = new ChromeAppCacheService;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              appcache_service_.get(),
              &ChromeAppCacheService::InitializeOnIOThread,
              GetPath(), IsOffTheRecord(),
              make_scoped_refptr(GetHostContentSettingsMap()),
              make_scoped_refptr(GetExtensionSpecialStoragePolicy()),
              false));
    }
    return appcache_service_;
  }

  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() {
    if (!db_tracker_.get()) {
      db_tracker_ = new webkit_database::DatabaseTracker(
          GetPath(), IsOffTheRecord(), GetExtensionSpecialStoragePolicy());
    }
    return db_tracker_;
  }

  virtual VisitedLinkMaster* GetVisitedLinkMaster() {
    // We don't provide access to the VisitedLinkMaster when we're OffTheRecord
    // because we don't want to leak the sites that the user has visited before.
    return NULL;
  }

  virtual ExtensionService* GetExtensionService() {
    return GetOriginalProfile()->GetExtensionService();
  }

  virtual BackgroundContentsService* GetBackgroundContentsService() const {
    return background_contents_service_.get();
  }

  virtual StatusTray* GetStatusTray() {
    return GetOriginalProfile()->GetStatusTray();
  }

  virtual UserScriptMaster* GetUserScriptMaster() {
    return GetOriginalProfile()->GetUserScriptMaster();
  }

  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager() {
    // TODO(mpcomplete): figure out whether we should return the original
    // profile's version.
    return NULL;
  }

  virtual ExtensionProcessManager* GetExtensionProcessManager() {
    return extension_process_manager_.get();
  }

  virtual ExtensionMessageService* GetExtensionMessageService() {
    return GetOriginalProfile()->GetExtensionMessageService();
  }

  virtual ExtensionEventRouter* GetExtensionEventRouter() {
    return GetOriginalProfile()->GetExtensionEventRouter();
  }

  virtual ExtensionIOEventRouter* GetExtensionIOEventRouter() {
    return GetOriginalProfile()->GetExtensionIOEventRouter();
  }

  virtual ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() {
    return GetOriginalProfile()->GetExtensionSpecialStoragePolicy();
  }

  virtual SSLHostState* GetSSLHostState() {
    if (!ssl_host_state_.get())
      ssl_host_state_.reset(new SSLHostState());

    DCHECK(ssl_host_state_->CalledOnValidThread());
    return ssl_host_state_.get();
  }

  virtual net::TransportSecurityState* GetTransportSecurityState() {
    if (!transport_security_state_.get())
      transport_security_state_ = new net::TransportSecurityState();

    return transport_security_state_.get();
  }

  virtual HistoryService* GetHistoryService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS)
      return profile_->GetHistoryService(sat);

    NOTREACHED() << "This profile is OffTheRecord";
    return NULL;
  }

  virtual HistoryService* GetHistoryServiceWithoutCreating() {
    return profile_->GetHistoryServiceWithoutCreating();
  }

  virtual FaviconService* GetFaviconService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS)
      return profile_->GetFaviconService(sat);

    NOTREACHED() << "This profile is OffTheRecord";
    return NULL;
  }

  virtual AutocompleteClassifier* GetAutocompleteClassifier() {
    return profile_->GetAutocompleteClassifier();
  }

  virtual WebDataService* GetWebDataService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS)
      return profile_->GetWebDataService(sat);

    NOTREACHED() << "This profile is OffTheRecord";
    return NULL;
  }

  virtual WebDataService* GetWebDataServiceWithoutCreating() {
    return profile_->GetWebDataServiceWithoutCreating();
  }

  virtual PasswordStore* GetPasswordStore(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS)
      return profile_->GetPasswordStore(sat);

    NOTREACHED() << "This profile is OffTheRecord";
    return NULL;
  }

  virtual PrefService* GetPrefs() {
    return prefs_;
  }

  virtual PrefService* GetOffTheRecordPrefs() {
    return prefs_;
  }

  virtual TemplateURLModel* GetTemplateURLModel() {
    return profile_->GetTemplateURLModel();
  }

  virtual TemplateURLFetcher* GetTemplateURLFetcher() {
    return profile_->GetTemplateURLFetcher();
  }

  virtual DownloadManager* GetDownloadManager() {
    if (!download_manager_.get()) {
      scoped_refptr<DownloadManager> dlm(
          new DownloadManager(g_browser_process->download_status_updater()));
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

  virtual fileapi::FileSystemContext* GetFileSystemContext() {
    if (!file_system_context_)
      file_system_context_ = CreateFileSystemContext(
          GetPath(), IsOffTheRecord(), GetExtensionSpecialStoragePolicy());
    DCHECK(file_system_context_.get());
    return file_system_context_.get();
  }

  virtual void InitThemes() {
    profile_->InitThemes();
  }

  virtual void SetTheme(const Extension* extension) {
    profile_->SetTheme(extension);
  }

  virtual void SetNativeTheme() {
    profile_->SetNativeTheme();
  }

  virtual void ClearTheme() {
    profile_->ClearTheme();
  }

  virtual const Extension* GetTheme() {
    return profile_->GetTheme();
  }

  virtual BrowserThemeProvider* GetThemeProvider() {
    return profile_->GetThemeProvider();
  }

  virtual URLRequestContextGetter* GetRequestContext() {
    return io_data_.GetMainRequestContextGetter();
  }

  virtual URLRequestContextGetter* GetRequestContextForMedia() {
    // In OTR mode, media request context is the same as the original one.
    return io_data_.GetMainRequestContextGetter();
  }

  URLRequestContextGetter* GetRequestContextForExtensions() {
    return io_data_.GetExtensionsRequestContextGetter();
  }

  virtual net::SSLConfigService* GetSSLConfigService() {
    return profile_->GetSSLConfigService();
  }

  virtual HostContentSettingsMap* GetHostContentSettingsMap() {
    // Retrieve the host content settings map of the parent profile in order to
    // ensure the preferences have been migrated.
    profile_->GetHostContentSettingsMap();
    if (!host_content_settings_map_.get())
      host_content_settings_map_ = new HostContentSettingsMap(this);
    return host_content_settings_map_.get();
  }

  virtual HostZoomMap* GetHostZoomMap() {
    if (!host_zoom_map_)
      host_zoom_map_ = new HostZoomMap(this);
    return host_zoom_map_.get();
  }

  virtual GeolocationContentSettingsMap* GetGeolocationContentSettingsMap() {
    return profile_->GetGeolocationContentSettingsMap();
  }

  virtual GeolocationPermissionContext* GetGeolocationPermissionContext() {
    return profile_->GetGeolocationPermissionContext();
  }

  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher() {
    return profile_->GetUserStyleSheetWatcher();
  }

  virtual FindBarState* GetFindBarState() {
    if (!find_bar_state_.get())
      find_bar_state_.reset(new FindBarState());
    return find_bar_state_.get();
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

  virtual bool HasProfileSyncService() const {
    // We never have a profile sync service.
    return false;
  }

  virtual bool DidLastSessionExitCleanly() {
    return profile_->DidLastSessionExitCleanly();
  }

  virtual BookmarkModel* GetBookmarkModel() {
    return profile_->GetBookmarkModel();
  }

  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry() {
    return profile_->GetProtocolHandlerRegistry();
  }

  virtual DesktopNotificationService* GetDesktopNotificationService() {
    if (!desktop_notification_service_.get()) {
      desktop_notification_service_.reset(new DesktopNotificationService(
          this, g_browser_process->notification_ui_manager()));
    }
    return desktop_notification_service_.get();
  }

  virtual TokenService* GetTokenService() {
    return NULL;
  }

  virtual ProfileSyncService* GetProfileSyncService() {
    return NULL;
  }

  virtual ProfileSyncService* GetProfileSyncService(
      const std::string& cros_user) {
    return NULL;
  }

  virtual BrowserSignin* GetBrowserSignin() {
    return profile_->GetBrowserSignin();
  }

  virtual CloudPrintProxyService* GetCloudPrintProxyService() {
    return NULL;
  }

  virtual bool IsSameProfile(Profile* profile) {
    return (profile == this) || (profile == profile_);
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
      webkit_context_ = new WebKitContext(this, false);
    DCHECK(webkit_context_.get());
    return webkit_context_.get();
  }

  virtual history::TopSites* GetTopSitesWithoutCreating() {
    return NULL;
  }

  virtual history::TopSites* GetTopSites() {
    return NULL;
  }

  virtual void MarkAsCleanShutdown() {
  }

  virtual void InitExtensions() {
    NOTREACHED();
  }

  virtual void InitPromoResources() {
    NOTREACHED();
  }

  virtual void InitRegisteredProtocolHandlers() {
    NOTREACHED();
  }

  virtual NTPResourceCache* GetNTPResourceCache() {
    // Just return the real profile resource cache.
    return profile_->GetNTPResourceCache();
  }

  virtual FilePath last_selected_directory() {
    const FilePath& directory = last_selected_directory_;
    if (directory.empty()) {
      return profile_->last_selected_directory();
    }
    return directory;
  }

  virtual void set_last_selected_directory(const FilePath& path) {
    last_selected_directory_ = path;
  }

#if defined(OS_CHROMEOS)
  virtual chromeos::ProxyConfigServiceImpl*
      GetChromeOSProxyConfigServiceImpl() {
    return profile_->GetChromeOSProxyConfigServiceImpl();
  }

  virtual void SetupChromeOSEnterpriseExtensionObserver() {
    profile_->SetupChromeOSEnterpriseExtensionObserver();
  }

  virtual void InitChromeOSPreferences() {
    // The off-the-record profile shouldn't have Chrome OS's preferences.
    // The preferences are associated with the regular user profile.
  }
#endif  // defined(OS_CHROMEOS)

  virtual void ExitedOffTheRecordMode() {
    // DownloadManager is lazily created, so check before accessing it.
    if (download_manager_.get()) {
      // Drop our download manager so we forget about all the downloads made
      // in off-the-record mode.
      download_manager_->Shutdown();
      download_manager_ = NULL;
    }
  }

  virtual void OnBrowserAdded(const Browser* browser) {
  }

  virtual void OnBrowserRemoved(const Browser* browser) {
    if (BrowserList::GetBrowserCount(this) == 0)
      ExitedOffTheRecordMode();
  }

  virtual ChromeBlobStorageContext* GetBlobStorageContext() {
    if (!blob_storage_context_) {
      blob_storage_context_ = new ChromeBlobStorageContext();
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              blob_storage_context_.get(),
              &ChromeBlobStorageContext::InitializeOnIOThread));
    }
    return blob_storage_context_;
  }

  virtual ExtensionInfoMap* GetExtensionInfoMap() {
    return profile_->GetExtensionInfoMap();
  }

  virtual policy::ProfilePolicyConnector* GetPolicyConnector() {
    return NULL;
  }

  virtual ChromeURLDataManager* GetChromeURLDataManager() {
    if (!chrome_url_data_manager_.get())
      chrome_url_data_manager_.reset(new ChromeURLDataManager(this));
    return chrome_url_data_manager_.get();
  }

  virtual PromoCounter* GetInstantPromoCounter() {
    return NULL;
  }

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) {
  }
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() {
    if (!pref_proxy_config_tracker_)
      pref_proxy_config_tracker_ = new PrefProxyConfigTracker(GetPrefs());

    return pref_proxy_config_tracker_;
  }

  virtual prerender::PrerenderManager* GetPrerenderManager() {
    // We do not allow prerendering in OTR profiles at this point.
    // TODO(tburkard): Figure out if we want to support this, and how, at some
    // point in the future.
    return NULL;
  }

 private:
  NotificationRegistrar registrar_;

  // The real underlying profile.
  Profile* profile_;

  // Weak pointer owned by |profile_|.
  PrefService* prefs_;

  scoped_ptr<ExtensionProcessManager> extension_process_manager_;

  OffTheRecordProfileIOData::Handle io_data_;

  // The download manager that only stores downloaded items in memory.
  scoped_refptr<DownloadManager> download_manager_;

  // Use a separate desktop notification service for OTR.
  scoped_ptr<DesktopNotificationService> desktop_notification_service_;

  // We use a non-writable content settings map for OTR.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  // Use a separate zoom map for OTR.
  scoped_refptr<HostZoomMap> host_zoom_map_;

  // Use a special WebKit context for OTR browsing.
  scoped_refptr<WebKitContext> webkit_context_;

  // We don't want SSLHostState from the OTR profile to leak back to the main
  // profile because then the main profile would learn some of the host names
  // the user visited while OTR.
  scoped_ptr<SSLHostState> ssl_host_state_;

  // Use a separate FindBarState so search terms do not leak back to the main
  // profile.
  scoped_ptr<FindBarState> find_bar_state_;

  // The TransportSecurityState that only stores enabled sites in memory.
  scoped_refptr<net::TransportSecurityState>
      transport_security_state_;

  // Time we were started.
  Time start_time_;

  scoped_refptr<ChromeAppCacheService> appcache_service_;

  // The main database tracker for this profile.
  // Should be used only on the file thread.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  FilePath last_selected_directory_;

  // Tracks all BackgroundContents running under this profile.
  scoped_ptr<BackgroundContentsService> background_contents_service_;

  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // The file_system context for this profile.
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  scoped_refptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_ptr<ChromeURLDataManager> chrome_url_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordProfileImpl);
};

#if defined(OS_CHROMEOS)
// Special case of the OffTheRecordProfileImpl which is used while Guest
// session in CrOS.
class GuestSessionProfile : public OffTheRecordProfileImpl {
 public:
  explicit GuestSessionProfile(Profile* real_profile)
      : OffTheRecordProfileImpl(real_profile) {
  }

  virtual PersonalDataManager* GetPersonalDataManager() {
    return GetOriginalProfile()->GetPersonalDataManager();
  }

  virtual void InitChromeOSPreferences() {
    chromeos_preferences_.reset(new chromeos::Preferences());
    chromeos_preferences_->Init(GetPrefs());
  }

 private:
  // The guest user should be able to customize Chrome OS preferences.
  scoped_ptr<chromeos::Preferences> chromeos_preferences_;
};
#endif

Profile* Profile::CreateOffTheRecordProfile() {
#if defined(OS_CHROMEOS)
  if (Profile::IsGuestSession())
    return new GuestSessionProfile(this);
#endif
  return new OffTheRecordProfileImpl(this);
}
