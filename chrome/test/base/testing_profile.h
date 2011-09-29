// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PROFILE_H_
#define CHROME_TEST_BASE_TESTING_PROFILE_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/appcache/chrome_appcache_service.h"

namespace content {
class ResourceContextGetter;
}

namespace history {
class TopSites;
}

namespace net {
class CookieMonster;
}

namespace quota {
class SpecialStoragePolicy;
}

class AutocompleteClassifier;
class BookmarkModel;
class CommandLine;
class ExtensionPrefs;
class ExtensionPrefStore;
class ExtensionPrefValueMap;
class ExtensionSpecialStoragePolicy;
class FaviconService;
class FindBarState;
class GeolocationPermissionContext;
class HistoryService;
class HostContentSettingsMap;
class PrefService;
class ProfileDependencyManager;
class ProfileSyncService;
class SpeechInputPreferences;
class TemplateURLService;
class TestingPrefService;
class ThemeService;
class WebKitContext;

namespace net {
class URLRequestContextGetter;
}

class TestingProfile : public Profile {
 public:
  // Default constructor that cannot be used with multi-profiles.
  TestingProfile();

  // Multi-profile aware constructor that takes the path to a directory managed
  // for this profile. This constructor is meant to be used by
  // TestingProfileManager::CreateTestingProfile. If you need to create multi-
  // profile profiles, use that factory method instead of this directly.
  explicit TestingProfile(const FilePath& path);

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

  // Creates a TemplateURLService. If not invoked the TemplateURLService is
  // NULL.  Creates a TemplateURLFetcher. If not invoked, the
  // TemplateURLFetcher is NULL.
  void CreateTemplateURLFetcher();

  // Creates a TemplateURLService. If not invoked, the TemplateURLService is
  // NULL.
  void CreateTemplateURLService();

  // Creates an ExtensionService initialized with the testing profile and
  // returns it. The profile keeps its own copy of a scoped_refptr to the
  // ExtensionService to make sure that is still alive to be notified when the
  // profile is destroyed.
  ExtensionService* CreateExtensionService(const CommandLine* command_line,
                                           const FilePath& install_directory,
                                           bool autoupdate_enabled);

  TestingPrefService* GetTestingPrefService();

  virtual TestingProfile* AsTestingProfile();
  virtual std::string GetProfileName();
  virtual FilePath GetPath();
  void set_incognito(bool incognito) { incognito_ = incognito; }
  virtual bool IsOffTheRecord();
  // Assumes ownership.
  virtual void SetOffTheRecordProfile(Profile* profile);
  virtual Profile* GetOffTheRecordProfile();
  virtual void DestroyOffTheRecordProfile() {}
  virtual bool HasOffTheRecordProfile();
  virtual Profile* GetOriginalProfile();
  void SetAppCacheService(ChromeAppCacheService* appcache_service);
  virtual ChromeAppCacheService* GetAppCacheService();
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker();
  virtual VisitedLinkMaster* GetVisitedLinkMaster();
  virtual ExtensionService* GetExtensionService();
  virtual UserScriptMaster* GetUserScriptMaster();
  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager();
  virtual ExtensionProcessManager* GetExtensionProcessManager();
  virtual ExtensionMessageService* GetExtensionMessageService();
  virtual ExtensionEventRouter* GetExtensionEventRouter();
  void SetExtensionSpecialStoragePolicy(
      ExtensionSpecialStoragePolicy* extension_special_storage_policy);
  virtual ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy();
  virtual SSLHostState* GetSSLHostState();
  virtual FaviconService* GetFaviconService(ServiceAccessType access);
  virtual HistoryService* GetHistoryService(ServiceAccessType access);
  virtual HistoryService* GetHistoryServiceWithoutCreating();
  // The CookieMonster will only be returned if a Context has been created. Do
  // this by calling CreateRequestContext(). See the note at GetRequestContext
  // for more information.
  net::CookieMonster* GetCookieMonster();
  virtual AutocompleteClassifier* GetAutocompleteClassifier();
  virtual history::ShortcutsBackend* GetShortcutsBackend();
  virtual WebDataService* GetWebDataService(ServiceAccessType access);
  virtual WebDataService* GetWebDataServiceWithoutCreating();
  virtual PasswordStore* GetPasswordStore(ServiceAccessType access);
  // Sets the profile's PrefService. If a pref service hasn't been explicitly
  // set GetPrefs creates one, so normally you need not invoke this. If you need
  // to set a pref service you must invoke this before GetPrefs.
  // TestingPrefService takes ownership of |prefs|.
  void SetPrefService(PrefService* prefs);
  virtual PrefService* GetPrefs();
  virtual TemplateURLFetcher* GetTemplateURLFetcher();
  virtual history::TopSites* GetTopSites();
  virtual history::TopSites* GetTopSitesWithoutCreating();
  virtual DownloadManager* GetDownloadManager();
  virtual fileapi::FileSystemContext* GetFileSystemContext();
  virtual void SetQuotaManager(quota::QuotaManager* manager);
  virtual quota::QuotaManager* GetQuotaManager();
  virtual bool HasCreatedDownloadManager() const;

  // Returns a testing ContextGetter (if one has been created via
  // CreateRequestContext) or NULL. This is not done on-demand for two reasons:
  // (1) Some tests depend on GetRequestContext() returning NULL. (2) Because
  // of the special memory management considerations for the
  // TestURLRequestContextGetter class, many tests would find themseleves
  // leaking if they called this method without the necessary IO thread. This
  // getter is currently only capable of returning a Context that helps test
  // the CookieMonster. See implementation comments for more details.
  virtual net::URLRequestContextGetter* GetRequestContext();
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id);
  void CreateRequestContext();
  // Clears out the created request context (which must be done before shutting
  // down the IO thread to avoid leaks).
  void ResetRequestContext();

  virtual net::URLRequestContextGetter* GetRequestContextForMedia();
  virtual net::URLRequestContextGetter* GetRequestContextForExtensions();
  virtual net::URLRequestContextGetter* GetRequestContextForIsolatedApp(
      const std::string& app_id);

  virtual const content::ResourceContext& GetResourceContext();

  virtual net::SSLConfigService* GetSSLConfigService();
  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher();
  virtual FindBarState* GetFindBarState();
  virtual HostContentSettingsMap* GetHostContentSettingsMap();
  virtual GeolocationPermissionContext* GetGeolocationPermissionContext();
  virtual SpeechInputPreferences* GetSpeechInputPreferences();
  virtual HostZoomMap* GetHostZoomMap();
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
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry();
  virtual SpellCheckHost* GetSpellCheckHost();
  virtual void ReinitializeSpellCheckHost(bool force) { }
  virtual WebKitContext* GetWebKitContext();
  virtual WebKitContext* GetOffTheRecordWebKitContext();
  virtual void MarkAsCleanShutdown() {}
  virtual void InitExtensions(bool extensions_enabled) {}
  virtual void InitPromoResources() {}
  virtual void InitRegisteredProtocolHandlers() {}

  virtual FilePath last_selected_directory();
  virtual void set_last_selected_directory(const FilePath& path);
#if defined(OS_CHROMEOS)
  virtual void SetupChromeOSEnterpriseExtensionObserver() {
  }
  virtual void InitChromeOSPreferences() {
  }
  virtual void ChangeAppLocale(const std::string&, AppLocaleChangedVia) {
  }
  virtual void OnLogin() {
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
  virtual ChromeBlobStorageContext* GetBlobStorageContext();
  virtual ExtensionInfoMap* GetExtensionInfoMap();
  virtual PromoCounter* GetInstantPromoCounter();
  virtual ChromeURLDataManager* GetChromeURLDataManager();
  virtual prerender::PrerenderManager* GetPrerenderManager();
  virtual chrome_browser_net::Predictor* GetNetworkPredictor();
  virtual void DeleteTransportSecurityStateSince(base::Time time);
  virtual PrefService* GetOffTheRecordPrefs();

  // TODO(jam): remove me once webkit_context_unittest.cc doesn't use Profile
  // and gets the quota::SpecialStoragePolicy* from whatever ends up replacing
  // it in the content module.
  quota::SpecialStoragePolicy* GetSpecialStoragePolicy();

 protected:
  base::Time start_time_;
  scoped_ptr<PrefService> prefs_;
  // ref only for right type, lifecycle is managed by prefs_
  TestingPrefService* testing_prefs_;

 private:
  virtual void SetDownloadManagerDelegate(
      ChromeDownloadManagerDelegate* delegate);

  // Common initialization between the two constructors.
  void Init();

  // Destroys favicon service if it has been created.
  void DestroyFaviconService();

  // If the webdata service has been created, it is destroyed.  This is invoked
  // from the destructor.
  void DestroyWebDataService();

  // Creates a TestingPrefService and associates it with the TestingProfile.
  void CreateTestingPrefService();

  // The favicon service. Only created if CreateFaviconService is invoked.
  scoped_ptr<FaviconService> favicon_service_;

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

  // Internally, this is a TestURLRequestContextGetter that creates a dummy
  // request context. Currently, only the CookieMonster is hooked up.
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_refptr<net::URLRequestContextGetter> extensions_request_context_;

  std::wstring id_;

  bool incognito_;
  scoped_ptr<Profile> incognito_profile_;

  // Did the last session exit cleanly? Default is true.
  bool last_session_exited_cleanly_;

  // FileSystemContext.  Created lazily by GetFileSystemContext().
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  // WebKitContext, lazily initialized by GetWebKitContext().
  scoped_refptr<WebKitContext> webkit_context_;

  // The main database tracker for this profile.
  // Should be used only on the file thread.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;

  scoped_refptr<SpeechInputPreferences> speech_input_preferences_;

  // Find bar state.  Created lazily by GetFindBarState().
  scoped_ptr<FindBarState> find_bar_state_;

  FilePath last_selected_directory_;
  scoped_refptr<history::TopSites> top_sites_;  // For history and thumbnails.

  // The Extension Preferences. Only created if CreateExtensionService is
  // invoked.
  scoped_ptr<ExtensionPrefs> extension_prefs_;

  scoped_ptr<ExtensionService> extension_service_;

  scoped_ptr<ExtensionPrefValueMap> extension_pref_value_map_;

  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;

  // The proxy prefs tracker.
  scoped_refptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  // We use a temporary directory to store testing profile data. In a multi-
  // profile environment, this is invalid and the directory is managed by the
  // TestingProfileManager.
  ScopedTempDir temp_dir_;
  // The path to this profile. This will be valid in either of the two above
  // cases.
  FilePath profile_path_;

  scoped_ptr<ChromeURLDataManager> chrome_url_data_manager_;

  scoped_ptr<prerender::PrerenderManager> prerender_manager_;

  // We keep a weak pointer to the dependency manager we want to notify on our
  // death. Defaults to the Singleton implementation but overridable for
  // testing.
  ProfileDependencyManager* profile_dependency_manager_;

  scoped_refptr<ChromeAppCacheService> appcache_service_;

  // The QuotaManager, only available if set explicitly via SetQuotaManager.
  scoped_refptr<quota::QuotaManager> quota_manager_;
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_H_
