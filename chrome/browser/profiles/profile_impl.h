// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#pragma once

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl_io_data.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ChromeDownloadManagerDelegate;
class ExtensionNavigationObserver;
class ExtensionPrefs;
class ExtensionPrefValueMap;
class ExtensionSettings;
class NetPrefObserver;
class PrefService;
class SpellCheckProfile;

#if defined(OS_CHROMEOS)
namespace chromeos {
class EnterpriseExtensionObserver;
class LocaleChangeGuard;
class Preferences;
}
#endif

// The default profile implementation.
class ProfileImpl : public Profile,
                    public NotificationObserver {
 public:
  virtual ~ProfileImpl();

  static void RegisterUserPrefs(PrefService* prefs);

  // Profile implementation.
  virtual std::string GetProfileName();
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
  virtual ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy();
  virtual FaviconService* GetFaviconService(ServiceAccessType sat);
  virtual HistoryService* GetHistoryService(ServiceAccessType sat);
  virtual HistoryService* GetHistoryServiceWithoutCreating();
  virtual AutocompleteClassifier* GetAutocompleteClassifier();
  virtual history::ShortcutsBackend* GetShortcutsBackend();
  virtual WebDataService* GetWebDataService(ServiceAccessType sat);
  virtual WebDataService* GetWebDataServiceWithoutCreating();
  virtual PasswordStore* GetPasswordStore(ServiceAccessType sat);
  virtual PrefService* GetPrefs();
  virtual PrefService* GetOffTheRecordPrefs();
  virtual TemplateURLFetcher* GetTemplateURLFetcher();
  virtual DownloadManager* GetDownloadManager();
  virtual PersonalDataManager* GetPersonalDataManager();
  virtual fileapi::FileSystemContext* GetFileSystemContext();
  virtual quota::QuotaManager* GetQuotaManager();
  virtual bool HasCreatedDownloadManager() const;
  virtual net::URLRequestContextGetter* GetRequestContext();
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id);
  virtual net::URLRequestContextGetter* GetRequestContextForMedia();
  virtual net::URLRequestContextGetter* GetRequestContextForExtensions();
  virtual net::URLRequestContextGetter* GetRequestContextForIsolatedApp(
      const std::string& app_id);
  virtual const content::ResourceContext& GetResourceContext();
  virtual void RegisterExtensionWithRequestContexts(const Extension* extension);
  virtual void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const extension_misc::UnloadedExtensionReason reason);
  virtual net::SSLConfigService* GetSSLConfigService();
  virtual HostContentSettingsMap* GetHostContentSettingsMap();
  virtual HostZoomMap* GetHostZoomMap();
  virtual GeolocationPermissionContext* GetGeolocationPermissionContext();
  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher();
  virtual FindBarState* GetFindBarState();
  virtual bool HasProfileSyncService() const;
  virtual bool DidLastSessionExitCleanly();
  virtual BookmarkModel* GetBookmarkModel();
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry();
  virtual bool IsSameProfile(Profile* profile);
  virtual base::Time GetStartTime() const;
  virtual SpellCheckHost* GetSpellCheckHost();
  virtual void ReinitializeSpellCheckHost(bool force);
  virtual WebKitContext* GetWebKitContext();
  virtual void MarkAsCleanShutdown();
  virtual void InitExtensions(bool extensions_enabled);
  virtual void InitPromoResources();
  virtual void InitRegisteredProtocolHandlers();
  virtual FilePath last_selected_directory();
  virtual void set_last_selected_directory(const FilePath& path);
  virtual ProfileSyncService* GetProfileSyncService();
  virtual ProfileSyncService* GetProfileSyncService(
      const std::string& cros_user);
  virtual TokenService* GetTokenService();
  void InitSyncService(const std::string& cros_user);
  virtual ChromeBlobStorageContext* GetBlobStorageContext();
  virtual ExtensionInfoMap* GetExtensionInfoMap();
  virtual PromoCounter* GetInstantPromoCounter();
  virtual ChromeURLDataManager* GetChromeURLDataManager();
  virtual chrome_browser_net::Predictor* GetNetworkPredictor();

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia);
  virtual void OnLogin();
  virtual void SetupChromeOSEnterpriseExtensionObserver();
  virtual void InitChromeOSPreferences();
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker();
  virtual prerender::PrerenderManager* GetPrerenderManager();

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class Profile;

  ProfileImpl(const FilePath& path,
              Profile::Delegate* delegate);

  // Does final initialization. Should be called after prefs were loaded.
  void DoFinalInit();

  // Does final prefs initialization and calls Init().
  void OnPrefsLoaded(bool success);

  void CreateWebDataService();
  FilePath GetPrefFilePath();

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
  LocalProfileId GetLocalProfileId();
#endif

  void CreatePasswordStore();

  void StopCreateSessionServiceTimer();

  void EnsureRequestContextCreated() {
    GetRequestContext();
  }

  void EnsureSessionServiceCreated();

  void RegisterComponentExtensions();

  void InstallDefaultApps();

  ExtensionPrefValueMap* GetExtensionPrefValueMap();

  void CreateQuotaManagerAndClients();

  SpellCheckProfile* GetSpellCheckProfile();

  virtual void SetDownloadManagerDelegate(
      ChromeDownloadManagerDelegate* delegate);

  NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  FilePath path_;
  FilePath base_cache_path_;
  scoped_ptr<ExtensionPrefValueMap> extension_pref_value_map_;
  // Keep |prefs_| on top for destruction order because |extension_prefs_|,
  // |net_pref_observer_|, |web_resource_service_|, and |io_data_| store
  // pointers to |prefs_| and shall be destructed first.
  scoped_ptr<PrefService> prefs_;
  scoped_ptr<PrefService> otr_prefs_;
  scoped_ptr<VisitedLinkEventListener> visited_link_event_listener_;
  scoped_ptr<VisitedLinkMaster> visited_link_master_;
  // Keep extension_prefs_ on top of extension_service_ because the latter
  // maintains a pointer to the first and shall be destructed first.
  scoped_ptr<ExtensionPrefs> extension_prefs_;
  scoped_refptr<ExtensionSettings> extension_settings_;
  scoped_ptr<ExtensionService> extension_service_;
  scoped_refptr<UserScriptMaster> user_script_master_;
  scoped_refptr<ExtensionDevToolsManager> extension_devtools_manager_;
  scoped_ptr<ExtensionProcessManager> extension_process_manager_;
  scoped_refptr<ExtensionMessageService> extension_message_service_;
  scoped_ptr<ExtensionEventRouter> extension_event_router_;
  scoped_ptr<ExtensionNavigationObserver> extension_navigation_observer_;
  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;
  scoped_ptr<SSLHostState> ssl_host_state_;
  scoped_refptr<net::TransportSecurityState>
      transport_security_state_;
  scoped_refptr<TransportSecurityPersister>
      transport_security_persister_;
  scoped_ptr<prerender::PrerenderManager> prerender_manager_;
  scoped_ptr<NetPrefObserver> net_pref_observer_;
  scoped_ptr<TemplateURLFetcher> template_url_fetcher_;
  scoped_ptr<BookmarkModel> bookmark_bar_model_;
  scoped_refptr<PromoResourceService> promo_resource_service_;
  scoped_refptr<ProtocolHandlerRegistry> protocol_handler_registry_;

  scoped_ptr<TokenService> token_service_;
  scoped_ptr<ProfileSyncFactory> profile_sync_factory_;
  scoped_ptr<ProfileSyncService> sync_service_;

  ProfileImplIOData::Handle io_data_;

  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<HostZoomMap> host_zoom_map_;
  scoped_refptr<GeolocationPermissionContext>
      geolocation_permission_context_;
  scoped_refptr<UserStyleSheetWatcher> user_style_sheet_watcher_;
  scoped_ptr<FindBarState> find_bar_state_;
  scoped_refptr<ChromeDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<DownloadManager> download_manager_;
  scoped_refptr<HistoryService> history_service_;
  scoped_refptr<FaviconService> favicon_service_;
  scoped_ptr<AutocompleteClassifier> autocomplete_classifier_;
  scoped_refptr<history::ShortcutsBackend> shortcuts_backend_;
  scoped_refptr<WebDataService> web_data_service_;
  scoped_refptr<PasswordStore> password_store_;
  scoped_refptr<WebKitContext> webkit_context_;
  scoped_refptr<PersonalDataManager> personal_data_manager_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  scoped_refptr<quota::QuotaManager> quota_manager_;
  bool history_service_created_;
  bool favicon_service_created_;
  bool created_web_data_service_;
  bool created_password_store_;
  bool created_download_manager_;
  bool clear_local_state_on_exit_;
  // Whether or not the last session exited cleanly. This is set only once.
  bool last_session_exited_cleanly_;

  base::OneShotTimer<ProfileImpl> create_session_service_timer_;

  scoped_ptr<Profile> off_the_record_profile_;

  // See GetStartTime for details.
  base::Time start_time_;

  scoped_ptr<SpellCheckProfile> spellcheck_profile_;

#if defined(OS_WIN)
  bool checked_instant_promo_;
  scoped_ptr<PromoCounter> instant_promo_counter_;
#endif

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

  scoped_ptr<chromeos::EnterpriseExtensionObserver>
      chromeos_enterprise_extension_observer_;

  scoped_ptr<chromeos::LocaleChangeGuard> locale_change_guard_;
#endif

  scoped_refptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_ptr<ChromeURLDataManager> chrome_url_data_manager_;

  Profile::Delegate* delegate_;

  chrome_browser_net::Predictor* predictor_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
