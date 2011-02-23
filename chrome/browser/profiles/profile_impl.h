// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#pragma once

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl_io_data.h"
#include "chrome/browser/spellcheck_host_observer.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class BackgroundModeManager;
class ExtensionPrefs;
class ExtensionPrefValueMap;
class PrefService;

#if defined(OS_CHROMEOS)
namespace chromeos {
class Preferences;
}
#endif

class NetPrefObserver;

// The default profile implementation.
class ProfileImpl : public Profile,
                    public SpellCheckHostObserver,
                    public NotificationObserver {
 public:
  virtual ~ProfileImpl();

  static void RegisterUserPrefs(PrefService* prefs);

  // Profile implementation.
  virtual ProfileId GetRuntimeId();
  virtual FilePath GetPath();
  virtual bool IsOffTheRecord();
  virtual Profile* GetOffTheRecordProfile();
  virtual void DestroyOffTheRecordProfile();
  virtual bool HasOffTheRecordProfile();
  virtual Profile* GetOriginalProfile();
  virtual ChromeAppCacheService* GetAppCacheService();
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker();
  virtual history::TopSites* GetTopSites();
  virtual history::TopSites* GetTopSitesWithoutCreating();
  virtual VisitedLinkMaster* GetVisitedLinkMaster();
  virtual UserScriptMaster* GetUserScriptMaster();
  virtual SSLHostState* GetSSLHostState();
  virtual net::TransportSecurityState* GetTransportSecurityState();
  virtual ExtensionService* GetExtensionService();
  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager();
  virtual ExtensionProcessManager* GetExtensionProcessManager();
  virtual ExtensionMessageService* GetExtensionMessageService();
  virtual ExtensionEventRouter* GetExtensionEventRouter();
  virtual ExtensionIOEventRouter* GetExtensionIOEventRouter();
  virtual FaviconService* GetFaviconService(ServiceAccessType sat);
  virtual HistoryService* GetHistoryService(ServiceAccessType sat);
  virtual HistoryService* GetHistoryServiceWithoutCreating();
  virtual AutocompleteClassifier* GetAutocompleteClassifier();
  virtual WebDataService* GetWebDataService(ServiceAccessType sat);
  virtual WebDataService* GetWebDataServiceWithoutCreating();
  virtual PasswordStore* GetPasswordStore(ServiceAccessType sat);
  virtual PrefService* GetPrefs();
  virtual PrefService* GetOffTheRecordPrefs();
  virtual TemplateURLModel* GetTemplateURLModel();
  virtual TemplateURLFetcher* GetTemplateURLFetcher();
  virtual DownloadManager* GetDownloadManager();
  virtual PersonalDataManager* GetPersonalDataManager();
  virtual fileapi::FileSystemContext* GetFileSystemContext();
  virtual void InitThemes();
  virtual void SetTheme(const Extension* extension);
  virtual void SetNativeTheme();
  virtual void ClearTheme();
  virtual const Extension* GetTheme();
  virtual BrowserThemeProvider* GetThemeProvider();
  virtual bool HasCreatedDownloadManager() const;
  virtual URLRequestContextGetter* GetRequestContext();
  virtual URLRequestContextGetter* GetRequestContextForMedia();
  virtual URLRequestContextGetter* GetRequestContextForExtensions();
  virtual void RegisterExtensionWithRequestContexts(const Extension* extension);
  virtual void UnregisterExtensionWithRequestContexts(
      const Extension* extension);
  virtual net::SSLConfigService* GetSSLConfigService();
  virtual HostContentSettingsMap* GetHostContentSettingsMap();
  virtual HostZoomMap* GetHostZoomMap();
  virtual GeolocationContentSettingsMap* GetGeolocationContentSettingsMap();
  virtual GeolocationPermissionContext* GetGeolocationPermissionContext();
  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher();
  virtual FindBarState* GetFindBarState();
  virtual SessionService* GetSessionService();
  virtual void ShutdownSessionService();
  virtual bool HasSessionService() const;
  virtual bool HasProfileSyncService() const;
  virtual bool DidLastSessionExitCleanly();
  virtual BookmarkModel* GetBookmarkModel();
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry();
  virtual bool IsSameProfile(Profile* profile);
  virtual base::Time GetStartTime() const;
  virtual TabRestoreService* GetTabRestoreService();
  virtual void ResetTabRestoreService();
  virtual SpellCheckHost* GetSpellCheckHost();
  virtual void ReinitializeSpellCheckHost(bool force);
  virtual WebKitContext* GetWebKitContext();
  virtual DesktopNotificationService* GetDesktopNotificationService();
  virtual BackgroundContentsService* GetBackgroundContentsService() const;
  virtual StatusTray* GetStatusTray();
  virtual void MarkAsCleanShutdown();
  virtual void InitExtensions();
  virtual void InitPromoResources();
  virtual void InitRegisteredProtocolHandlers();
  virtual NTPResourceCache* GetNTPResourceCache();
  virtual FilePath last_selected_directory();
  virtual void set_last_selected_directory(const FilePath& path);
  virtual ProfileSyncService* GetProfileSyncService();
  virtual ProfileSyncService* GetProfileSyncService(
      const std::string& cros_user);
  virtual TokenService* GetTokenService();
  void InitSyncService(const std::string& cros_user);
  virtual CloudPrintProxyService* GetCloudPrintProxyService();
  void InitCloudPrintProxyService();
  virtual ChromeBlobStorageContext* GetBlobStorageContext();
  virtual ExtensionInfoMap* GetExtensionInfoMap();
  virtual PromoCounter* GetInstantPromoCounter();
  virtual BrowserSignin* GetBrowserSignin();
  virtual policy::ProfilePolicyConnector* GetPolicyConnector();
  virtual ChromeURLDataManager* GetChromeURLDataManager();

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia);
  virtual chromeos::ProxyConfigServiceImpl* GetChromeOSProxyConfigServiceImpl();
  virtual void SetupChromeOSEnterpriseExtensionObserver();
  virtual void InitChromeOSPreferences();
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker();
  virtual prerender::PrerenderManager* GetPrerenderManager();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // SpellCheckHostObserver implementation.
  virtual void SpellCheckHostInitialized();

 private:
  friend class Profile;

  explicit ProfileImpl(const FilePath& path);

  void CreateWebDataService();
  FilePath GetPrefFilePath();

  void CreatePasswordStore();

  void StopCreateSessionServiceTimer();

  void EnsureRequestContextCreated() {
    GetRequestContext();
  }

  void EnsureSessionServiceCreated() {
    GetSessionService();
  }

  void RegisterComponentExtensions();
  void InstallDefaultApps();

  ExtensionPrefValueMap* GetExtensionPrefValueMap();

  NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  FilePath path_;
  FilePath base_cache_path_;
  scoped_ptr<ExtensionPrefValueMap> extension_pref_value_map_;
  // Keep prefs_ on top for destruction order because extension_prefs_,
  // net_pref_observer_, web_resource_service_ and background_contents_service_
  // store pointers to prefs_ and shall be destructed first.
  scoped_ptr<PrefService> prefs_;
  scoped_ptr<PrefService> otr_prefs_;
  scoped_ptr<VisitedLinkEventListener> visited_link_event_listener_;
  scoped_ptr<VisitedLinkMaster> visited_link_master_;
  // Keep extension_prefs_ on top of extensions_service_ because the latter
  // maintains a pointer to the first and shall be destructed first.
  scoped_ptr<ExtensionPrefs> extension_prefs_;
  scoped_refptr<ExtensionService> extensions_service_;
  scoped_refptr<UserScriptMaster> user_script_master_;
  scoped_refptr<ExtensionDevToolsManager> extension_devtools_manager_;
  scoped_ptr<ExtensionProcessManager> extension_process_manager_;
  scoped_refptr<ExtensionMessageService> extension_message_service_;
  scoped_ptr<ExtensionEventRouter> extension_event_router_;
  scoped_refptr<ExtensionIOEventRouter> extension_io_event_router_;
  scoped_ptr<SSLHostState> ssl_host_state_;
  scoped_refptr<net::TransportSecurityState>
      transport_security_state_;
  scoped_refptr<TransportSecurityPersister>
      transport_security_persister_;
  scoped_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;
  scoped_ptr<NetPrefObserver> net_pref_observer_;
  scoped_ptr<TemplateURLFetcher> template_url_fetcher_;
  scoped_ptr<TemplateURLModel> template_url_model_;
  scoped_ptr<BookmarkModel> bookmark_bar_model_;
  scoped_refptr<PromoResourceService> promo_resource_service_;
  scoped_refptr<ProtocolHandlerRegistry> protocol_handler_registry_;
  scoped_ptr<NTPResourceCache> ntp_resource_cache_;

  scoped_ptr<TokenService> token_service_;
  scoped_ptr<ProfileSyncFactory> profile_sync_factory_;
  scoped_ptr<ProfileSyncService> sync_service_;
  scoped_refptr<CloudPrintProxyService> cloud_print_proxy_service_;

  ProfileImplIOData::Handle io_data_;

  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<HostZoomMap> host_zoom_map_;
  scoped_refptr<GeolocationContentSettingsMap>
      geolocation_content_settings_map_;
  scoped_refptr<GeolocationPermissionContext>
      geolocation_permission_context_;
  scoped_refptr<UserStyleSheetWatcher> user_style_sheet_watcher_;
  scoped_ptr<FindBarState> find_bar_state_;
  scoped_refptr<DownloadManager> download_manager_;
  scoped_refptr<HistoryService> history_service_;
  scoped_refptr<FaviconService> favicon_service_;
  scoped_ptr<AutocompleteClassifier> autocomplete_classifier_;
  scoped_refptr<WebDataService> web_data_service_;
  scoped_refptr<PasswordStore> password_store_;
  scoped_refptr<SessionService> session_service_;
  scoped_ptr<BrowserThemeProvider> theme_provider_;
  scoped_refptr<WebKitContext> webkit_context_;
  scoped_ptr<DesktopNotificationService> desktop_notification_service_;
  scoped_ptr<BackgroundContentsService> background_contents_service_;
  scoped_ptr<BackgroundModeManager> background_mode_manager_;
  scoped_ptr<StatusTray> status_tray_;
  scoped_refptr<PersonalDataManager> personal_data_manager_;
  scoped_ptr<PinnedTabService> pinned_tab_service_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  scoped_ptr<BrowserSignin> browser_signin_;
  bool history_service_created_;
  bool favicon_service_created_;
  bool created_web_data_service_;
  bool created_password_store_;
  bool created_download_manager_;
  bool created_theme_provider_;
  bool clear_local_state_on_exit_;
  // Whether or not the last session exited cleanly. This is set only once.
  bool last_session_exited_cleanly_;

  base::OneShotTimer<ProfileImpl> create_session_service_timer_;

  scoped_ptr<Profile> off_the_record_profile_;

  // See GetStartTime for details.
  base::Time start_time_;

  scoped_refptr<TabRestoreService> tab_restore_service_;

  scoped_refptr<SpellCheckHost> spellcheck_host_;

  // Indicates whether |spellcheck_host_| has told us initialization is
  // finished.
  bool spellcheck_host_ready_;

#if defined(OS_WIN)
  bool checked_instant_promo_;
  scoped_ptr<PromoCounter> instant_promo_counter_;
#endif

  // Set to true when ShutdownSessionService is invoked. If true
  // GetSessionService won't recreate the SessionService.
  bool shutdown_session_service_;

  // The AppCacheService for this profile, shared by all requests contexts
  // associated with this profile. Should only be used on the IO thread.
  scoped_refptr<ChromeAppCacheService> appcache_service_;

  // The main database tracker for this profile.
  // Should be used only on the file thread.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  scoped_refptr<history::TopSites> top_sites_;  // For history and thumbnails.

  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  scoped_refptr<ExtensionInfoMap> extension_info_map_;

#if defined(OS_CHROMEOS)
  scoped_ptr<chromeos::Preferences> chromeos_preferences_;

  scoped_refptr<chromeos::ProxyConfigServiceImpl>
      chromeos_proxy_config_service_impl_;

  scoped_ptr<chromeos::EnterpriseExtensionObserver>
      chromeos_enterprise_extension_observer_;
#endif

  scoped_refptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_refptr<prerender::PrerenderManager> prerender_manager_;

  scoped_ptr<ChromeURLDataManager> chrome_url_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
