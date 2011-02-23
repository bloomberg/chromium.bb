// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_PROFILE_H_
#define CHROME_TEST_TESTING_PROFILE_H_
#pragma once

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/timer.h"
#include "chrome/browser/profiles/profile.h"

namespace history {
class TopSites;
}

namespace net {
class CookieMonster;
}

class AutocompleteClassifier;
class BookmarkModel;
class BrowserThemeProvider;
class CommandLine;
class DesktopNotificationService;
class ExtensionPrefs;
class ExtensionPrefStore;
class ExtensionPrefValueMap;
class FaviconService;
class FindBarState;
class GeolocationContentSettingsMap;
class GeolocationPermissionContext;
class HistoryService;
class HostContentSettingsMap;
class PrefService;
class ProfileSyncService;
class SessionService;
class TemplateURLModel;
class TestingPrefService;
class URLRequestContextGetter;
class WebKitContext;

class TestingProfile : public Profile {
 public:
  TestingProfile();

  virtual ~TestingProfile();

  // Creates the favicon service. Consequent calls would recreate the service.
  void CreateFaviconService();

  // Creates the history service. If |delete_file| is true, the history file is
  // deleted first, then the HistoryService is created. As TestingProfile
  // deletes the directory containing the files used by HistoryService, this
  // only matters if you're recreating the HistoryService.  If |no_db| is true,
  // the history backend will fail to initialize its database; this is useful
  // for testing error conditions.
  void CreateHistoryService(bool delete_file, bool no_db);

  // Shuts down and nulls out the reference to HistoryService.
  void DestroyHistoryService();

  // Creates TopSites. This returns immediately, and top sites may not be
  // loaded. Use BlockUntilTopSitesLoaded to ensure TopSites has finished
  // loading.
  void CreateTopSites();

  // Shuts down and nulls out the reference to TopSites.
  void DestroyTopSites();

  // Creates the BookmkarBarModel. If not invoked the bookmark bar model is
  // NULL. If |delete_file| is true, the bookmarks file is deleted first, then
  // the model is created. As TestingProfile deletes the directory containing
  // the files used by HistoryService, the boolean only matters if you're
  // recreating the BookmarkModel.
  //
  // NOTE: this does not block until the bookmarks are loaded. For that use
  // BlockUntilBookmarkModelLoaded.
  void CreateBookmarkModel(bool delete_file);

  // Creates an AutocompleteClassifier. If not invoked the
  // AutocompleteClassifier is NULL.
  void CreateAutocompleteClassifier();

  // Creates a ProtocolHandlerRegistry. If not invoked the protocol handler
  // registry is NULL.
  void CreateProtocolHandlerRegistry();

  // Creates the webdata service.  If |delete_file| is true, the webdata file is
  // deleted first, then the WebDataService is created.  As TestingProfile
  // deletes the directory containing the files used by WebDataService, this
  // only matters if you're recreating the WebDataService.
  void CreateWebDataService(bool delete_file);

  // Blocks until the BookmarkModel finishes loaded. This is NOT invoked from
  // CreateBookmarkModel.
  void BlockUntilBookmarkModelLoaded();

  // Blocks until TopSites finishes loading.
  void BlockUntilTopSitesLoaded();

  // Creates a TemplateURLModel. If not invoked the TemplateURLModel is NULL.
  // Creates a TemplateURLFetcher. If not invoked, the TemplateURLFetcher is
  // NULL.
  void CreateTemplateURLFetcher();

  // Creates a TemplateURLModel. If not invoked, the TemplateURLModel is NULL.
  void CreateTemplateURLModel();

  // Sets the TemplateURLModel. Takes ownership of it.
  void SetTemplateURLModel(TemplateURLModel* model);

  // Uses a specific theme provider for this profile. TestingProfile takes
  // ownership of |theme_provider|.
  void UseThemeProvider(BrowserThemeProvider* theme_provider);

  // Creates an ExtensionService initialized with the testing profile and
  // returns it. The profile keeps its own copy of a scoped_refptr to the
  // ExtensionService to make sure that is still alive to be notified when the
  // profile is destroyed.
  ExtensionService* CreateExtensionService(const CommandLine* command_line,
                                           const FilePath& install_directory);

  TestingPrefService* GetTestingPrefService();

  virtual ProfileId GetRuntimeId();

  virtual FilePath GetPath();

  // Sets whether we're off the record. Default is false.
  void set_off_the_record(bool off_the_record) {
    off_the_record_ = off_the_record;
  }
  virtual bool IsOffTheRecord();
  virtual Profile* GetOffTheRecordProfile();

  virtual void DestroyOffTheRecordProfile() {}

  virtual bool HasOffTheRecordProfile();

  virtual Profile* GetOriginalProfile();
  virtual ChromeAppCacheService* GetAppCacheService();
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker();
  virtual VisitedLinkMaster* GetVisitedLinkMaster();
  virtual ExtensionService* GetExtensionService();
  virtual UserScriptMaster* GetUserScriptMaster();
  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager();
  virtual ExtensionProcessManager* GetExtensionProcessManager();
  virtual ExtensionMessageService* GetExtensionMessageService();
  virtual ExtensionEventRouter* GetExtensionEventRouter();
  virtual ExtensionIOEventRouter* GetExtensionIOEventRouter();
  virtual SSLHostState* GetSSLHostState();
  virtual net::TransportSecurityState* GetTransportSecurityState();
  virtual FaviconService* GetFaviconService(ServiceAccessType access);
  virtual HistoryService* GetHistoryService(ServiceAccessType access);
  virtual HistoryService* GetHistoryServiceWithoutCreating();
  void set_has_history_service(bool has_history_service) {
    has_history_service_ = has_history_service;
  }
  // The CookieMonster will only be returned if a Context has been created. Do
  // this by calling CreateRequestContext(). See the note at GetRequestContext
  // for more information.
  net::CookieMonster* GetCookieMonster();
  virtual AutocompleteClassifier* GetAutocompleteClassifier();
  virtual WebDataService* GetWebDataService(ServiceAccessType access);
  virtual WebDataService* GetWebDataServiceWithoutCreating();
  virtual PasswordStore* GetPasswordStore(ServiceAccessType access);
  // Sets the profile's PrefService. If a pref service hasn't been explicitly
  // set GetPrefs creates one, so normally you need not invoke this. If you need
  // to set a pref service you must invoke this before GetPrefs.
  // TestingPrefService takes ownership of |prefs|.
  void SetPrefService(PrefService* prefs);
  virtual PrefService* GetPrefs();
  virtual TemplateURLModel* GetTemplateURLModel();
  virtual TemplateURLFetcher* GetTemplateURLFetcher();
  virtual history::TopSites* GetTopSites();
  virtual history::TopSites* GetTopSitesWithoutCreating();
  virtual DownloadManager* GetDownloadManager();
  virtual PersonalDataManager* GetPersonalDataManager();
  virtual fileapi::FileSystemContext* GetFileSystemContext();
  virtual BrowserSignin* GetBrowserSignin();
  virtual bool HasCreatedDownloadManager() const;
  virtual void InitThemes();
  virtual void SetTheme(const Extension* extension) {}
  virtual void SetNativeTheme() {}
  virtual void ClearTheme() {}
  virtual const Extension* GetTheme();
  virtual BrowserThemeProvider* GetThemeProvider();

  // Returns a testing ContextGetter (if one has been created via
  // CreateRequestContext) or NULL. This is not done on-demand for two reasons:
  // (1) Some tests depend on GetRequestContext() returning NULL. (2) Because
  // of the special memory management considerations for the
  // TestURLRequestContextGetter class, many tests would find themseleves
  // leaking if they called this method without the necessary IO thread. This
  // getter is currently only capable of returning a Context that helps test
  // the CookieMonster. See implementation comments for more details.
  virtual URLRequestContextGetter* GetRequestContext();
  void CreateRequestContext();
  // Clears out the created request context (which must be done before shutting
  // down the IO thread to avoid leaks).
  void ResetRequestContext();

  virtual URLRequestContextGetter* GetRequestContextForMedia();
  virtual URLRequestContextGetter* GetRequestContextForExtensions();

  virtual net::SSLConfigService* GetSSLConfigService();
  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher();
  virtual FindBarState* GetFindBarState();
  virtual HostContentSettingsMap* GetHostContentSettingsMap();
  virtual GeolocationContentSettingsMap* GetGeolocationContentSettingsMap();
  virtual GeolocationPermissionContext* GetGeolocationPermissionContext();
  virtual HostZoomMap* GetHostZoomMap();
  void set_session_service(SessionService* session_service);
  virtual SessionService* GetSessionService();
  virtual void ShutdownSessionService() {}
  virtual bool HasSessionService() const;
  virtual bool HasProfileSyncService() const;
  virtual std::wstring GetName();
  virtual void SetName(const std::wstring& name) {}
  virtual std::wstring GetID();
  virtual void SetID(const std::wstring& id);
  void set_last_session_exited_cleanly(bool value) {
    last_session_exited_cleanly_ = value;
  }
  virtual bool DidLastSessionExitCleanly();
  virtual void MergeResourceString(int message_id,
                                   std::wstring* output_string) {}
  virtual void MergeResourceInteger(int message_id, int* output_value) {}
  virtual void MergeResourceBoolean(int message_id, bool* output_value) {}
  virtual BookmarkModel* GetBookmarkModel();
  virtual bool IsSameProfile(Profile *p);
  virtual base::Time GetStartTime() const;
  virtual TabRestoreService* GetTabRestoreService();
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry();
  virtual void ResetTabRestoreService() {}
  virtual SpellCheckHost* GetSpellCheckHost();
  virtual void ReinitializeSpellCheckHost(bool force) { }
  virtual WebKitContext* GetWebKitContext();
  virtual WebKitContext* GetOffTheRecordWebKitContext();
  virtual void MarkAsCleanShutdown() {}
  virtual void InitExtensions() {}
  virtual void InitPromoResources() {}
  virtual void InitRegisteredProtocolHandlers() {}
  virtual NTPResourceCache* GetNTPResourceCache();

  virtual DesktopNotificationService* GetDesktopNotificationService();
  virtual BackgroundContentsService* GetBackgroundContentsService() const;
  virtual StatusTray* GetStatusTray();
  virtual FilePath last_selected_directory();
  virtual void set_last_selected_directory(const FilePath& path);
#if defined(OS_CHROMEOS)
  virtual chromeos::ProxyConfigServiceImpl*
      GetChromeOSProxyConfigServiceImpl() {
    return NULL;
  }
  virtual void SetupChromeOSEnterpriseExtensionObserver() {
  }
  virtual void InitChromeOSPreferences() {
  }
  virtual void ChangeAppLocale(const std::string&, AppLocaleChangedVia) {
  }
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker();

  // Schedules a task on the history backend and runs a nested loop until the
  // task is processed.  This has the effect of blocking the caller until the
  // history service processes all pending requests.
  void BlockUntilHistoryProcessesPendingRequests();

  // Creates and initializes a profile sync service if the tests require one.
  virtual TokenService* GetTokenService();
  virtual ProfileSyncService* GetProfileSyncService();
  virtual ProfileSyncService* GetProfileSyncService(
      const std::string& cros_notes);
  virtual CloudPrintProxyService* GetCloudPrintProxyService();
  virtual ChromeBlobStorageContext* GetBlobStorageContext();
  virtual ExtensionInfoMap* GetExtensionInfoMap();
  virtual PromoCounter* GetInstantPromoCounter();
  virtual policy::ProfilePolicyConnector* GetPolicyConnector();
  virtual ChromeURLDataManager* GetChromeURLDataManager();
  virtual prerender::PrerenderManager* GetPrerenderManager();
  virtual PrefService* GetOffTheRecordPrefs();

 protected:
  base::Time start_time_;
  scoped_ptr<PrefService> prefs_;
  // ref only for right type, lifecycle is managed by prefs_
  TestingPrefService* testing_prefs_;

 private:
  // Destroys favicon service if it has been created.
  void DestroyFaviconService();

  // If the webdata service has been created, it is destroyed.  This is invoked
  // from the destructor.
  void DestroyWebDataService();

  // Creates a TestingPrefService and associates it with the TestingProfile.
  void CreateTestingPrefService();

  // The favicon service. Only created if CreateFaviconService is invoked.
  scoped_refptr<FaviconService> favicon_service_;

  // The history service. Only created if CreateHistoryService is invoked.
  scoped_refptr<HistoryService> history_service_;

  // The BookmarkModel. Only created if CreateBookmarkModel is invoked.
  scoped_ptr<BookmarkModel> bookmark_bar_model_;

  // The ProtocolHandlerRegistry. Only created if CreateProtocolHandlerRegistry
  // is invoked.
  scoped_refptr<ProtocolHandlerRegistry> protocol_handler_registry_;

  // The TokenService. Created by CreateTokenService. Filled with dummy data.
  scoped_ptr<TokenService> token_service_;

  // The ProfileSyncService.  Created by CreateProfileSyncService.
  scoped_ptr<ProfileSyncService> profile_sync_service_;

  // The AutocompleteClassifier.  Only created if CreateAutocompleteClassifier
  // is invoked.
  scoped_ptr<AutocompleteClassifier> autocomplete_classifier_;

  // The WebDataService.  Only created if CreateWebDataService is invoked.
  scoped_refptr<WebDataService> web_data_service_;

  // The TemplateURLFetcher. Only created if CreateTemplateURLFetcher is
  // invoked.
  scoped_ptr<TemplateURLFetcher> template_url_fetcher_;

  // The TemplateURLModel. Only created if CreateTemplateURLModel is invoked.
  scoped_ptr<TemplateURLModel> template_url_model_;

  scoped_ptr<NTPResourceCache> ntp_resource_cache_;

  // The SessionService. Defaults to NULL, but can be set using the setter.
  scoped_refptr<SessionService> session_service_;

  // The theme provider. Created lazily by GetThemeProvider()/InitThemes().
  scoped_ptr<BrowserThemeProvider> theme_provider_;
  bool created_theme_provider_;

  // Internally, this is a TestURLRequestContextGetter that creates a dummy
  // request context. Currently, only the CookieMonster is hooked up.
  scoped_refptr<URLRequestContextGetter> request_context_;
  scoped_refptr<URLRequestContextGetter> extensions_request_context_;

  // Do we have a history service? This defaults to the value of
  // history_service, but can be explicitly set.
  bool has_history_service_;

  std::wstring id_;

  bool off_the_record_;

  // Did the last session exit cleanly? Default is true.
  bool last_session_exited_cleanly_;

  // The main database tracker for this profile.
  // Should be used only on the file thread.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  // WebKitContext, lazily initialized by GetWebKitContext().
  scoped_refptr<WebKitContext> webkit_context_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<GeolocationContentSettingsMap>
      geolocation_content_settings_map_;
  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;
  scoped_ptr<DesktopNotificationService> desktop_notification_service_;

  // Find bar state.  Created lazily by GetFindBarState().
  scoped_ptr<FindBarState> find_bar_state_;

  FilePath last_selected_directory_;
  scoped_refptr<history::TopSites> top_sites_;  // For history and thumbnails.

  // The Extension Preferences. Only created if CreateExtensionService is
  // invoked.
  scoped_ptr<ExtensionPrefs> extension_prefs_;

  // For properly notifying the ExtensionService when the profile
  // is disposed.
  scoped_refptr<ExtensionService> extensions_service_;

  scoped_ptr<ExtensionPrefValueMap> extension_pref_value_map_;

  // The proxy prefs tracker.
  scoped_refptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  // We use a temporary directory to store testing profile data.
  ScopedTempDir temp_dir_;

  scoped_ptr<ChromeURLDataManager> chrome_url_data_manager_;
};

// A profile that derives from another profile.  This does not actually
// override anything except the GetRuntimeId() in order to test sharing of
// site information.
class DerivedTestingProfile : public TestingProfile {
 public:
  explicit DerivedTestingProfile(Profile* profile);
  virtual ~DerivedTestingProfile();

  virtual ProfileId GetRuntimeId();

 protected:
  Profile* original_profile_;
};

#endif  // CHROME_TEST_TESTING_PROFILE_H_
