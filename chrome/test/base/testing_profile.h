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

  // Creates an ExtensionProcessManager. If not invoked, the
  // ExtensionProcessManager is NULL.
  void CreateExtensionProcessManager();

  // Creates an ExtensionService initialized with the testing profile and
  // returns it. The profile keeps its own copy of a scoped_refptr to the
  // ExtensionService to make sure that is still alive to be notified when the
  // profile is destroyed.
  ExtensionService* CreateExtensionService(const CommandLine* command_line,
                                           const FilePath& install_directory,
                                           bool autoupdate_enabled);

  TestingPrefService* GetTestingPrefService();

  virtual TestingProfile* AsTestingProfile() OVERRIDE;
  virtual std::string GetProfileName() OVERRIDE;
  virtual FilePath GetPath() OVERRIDE;
  void set_incognito(bool incognito) { incognito_ = incognito; }
  virtual bool IsOffTheRecord() OVERRIDE;
  // Assumes ownership.
  virtual void SetOffTheRecordProfile(Profile* profile);
  virtual Profile* GetOffTheRecordProfile() OVERRIDE;
  virtual void DestroyOffTheRecordProfile() OVERRIDE {}
  virtual bool HasOffTheRecordProfile() OVERRIDE;
  virtual Profile* GetOriginalProfile() OVERRIDE;
  void SetAppCacheService(ChromeAppCacheService* appcache_service);
  virtual ChromeAppCacheService* GetAppCacheService() OVERRIDE;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() OVERRIDE;
  virtual VisitedLinkMaster* GetVisitedLinkMaster() OVERRIDE;
  virtual ExtensionService* GetExtensionService() OVERRIDE;
  virtual UserScriptMaster* GetUserScriptMaster() OVERRIDE;
  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager() OVERRIDE;
  virtual ExtensionProcessManager* GetExtensionProcessManager() OVERRIDE;
  virtual ExtensionMessageService* GetExtensionMessageService() OVERRIDE;
  virtual ExtensionEventRouter* GetExtensionEventRouter() OVERRIDE;
  void SetExtensionSpecialStoragePolicy(
      ExtensionSpecialStoragePolicy* extension_special_storage_policy);
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() OVERRIDE;
  virtual SSLHostState* GetSSLHostState() OVERRIDE;
  virtual FaviconService* GetFaviconService(ServiceAccessType access) OVERRIDE;
  virtual HistoryService* GetHistoryService(ServiceAccessType access) OVERRIDE;
  virtual HistoryService* GetHistoryServiceWithoutCreating() OVERRIDE;
  // The CookieMonster will only be returned if a Context has been created. Do
  // this by calling CreateRequestContext(). See the note at GetRequestContext
  // for more information.
  net::CookieMonster* GetCookieMonster();
  virtual AutocompleteClassifier* GetAutocompleteClassifier() OVERRIDE;
  virtual history::ShortcutsBackend* GetShortcutsBackend() OVERRIDE;
  virtual WebDataService* GetWebDataService(ServiceAccessType access) OVERRIDE;
  virtual WebDataService* GetWebDataServiceWithoutCreating() OVERRIDE;
  virtual PasswordStore* GetPasswordStore(ServiceAccessType access) OVERRIDE;
  // Sets the profile's PrefService. If a pref service hasn't been explicitly
  // set GetPrefs creates one, so normally you need not invoke this. If you need
  // to set a pref service you must invoke this before GetPrefs.
  // TestingPrefService takes ownership of |prefs|.
  void SetPrefService(PrefService* prefs);
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual TemplateURLFetcher* GetTemplateURLFetcher() OVERRIDE;
  virtual history::TopSites* GetTopSites() OVERRIDE;
  virtual history::TopSites* GetTopSitesWithoutCreating() OVERRIDE;
  virtual DownloadManager* GetDownloadManager() OVERRIDE;
  virtual fileapi::FileSystemContext* GetFileSystemContext() OVERRIDE;
  virtual void SetQuotaManager(quota::QuotaManager* manager);
  virtual quota::QuotaManager* GetQuotaManager() OVERRIDE;

  // Returns a testing ContextGetter (if one has been created via
  // CreateRequestContext) or NULL. This is not done on-demand for two reasons:
  // (1) Some tests depend on GetRequestContext() returning NULL. (2) Because
  // of the special memory management considerations for the
  // TestURLRequestContextGetter class, many tests would find themseleves
  // leaking if they called this method without the necessary IO thread. This
  // getter is currently only capable of returning a Context that helps test
  // the CookieMonster. See implementation comments for more details.
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  void CreateRequestContext();
  // Clears out the created request context (which must be done before shutting
  // down the IO thread to avoid leaks).
  void ResetRequestContext();

  virtual net::URLRequestContextGetter* GetRequestContextForMedia() OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForIsolatedApp(
      const std::string& app_id) OVERRIDE;

  virtual const content::ResourceContext& GetResourceContext() OVERRIDE;

  virtual net::SSLConfigService* GetSSLConfigService() OVERRIDE;
  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher() OVERRIDE;
  virtual FindBarState* GetFindBarState() OVERRIDE;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() OVERRIDE;
  virtual GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual SpeechInputPreferences* GetSpeechInputPreferences() OVERRIDE;
  virtual HostZoomMap* GetHostZoomMap() OVERRIDE;
  virtual bool HasProfileSyncService() const OVERRIDE;
  virtual std::wstring GetName();
  virtual void SetName(const std::wstring& name) {}
  virtual std::wstring GetID();
  virtual void SetID(const std::wstring& id);
  void set_last_session_exited_cleanly(bool value) {
    last_session_exited_cleanly_ = value;
  }
  virtual bool DidLastSessionExitCleanly() OVERRIDE;
  virtual void MergeResourceString(int message_id,
                                   std::wstring* output_string) {}
  virtual void MergeResourceInteger(int message_id, int* output_value) {}
  virtual void MergeResourceBoolean(int message_id, bool* output_value) {}
  virtual BookmarkModel* GetBookmarkModel() OVERRIDE;
  virtual bool IsSameProfile(Profile *p) OVERRIDE;
  virtual base::Time GetStartTime() const OVERRIDE;
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry() OVERRIDE;
  virtual SpellCheckHost* GetSpellCheckHost() OVERRIDE;
  virtual void ReinitializeSpellCheckHost(bool force) OVERRIDE { }
  virtual WebKitContext* GetWebKitContext() OVERRIDE;
  virtual WebKitContext* GetOffTheRecordWebKitContext();
  virtual void MarkAsCleanShutdown() OVERRIDE {}
  virtual void InitExtensions(bool extensions_enabled) OVERRIDE {}
  virtual void InitPromoResources() OVERRIDE {}
  virtual void InitRegisteredProtocolHandlers() OVERRIDE {}

  virtual FilePath last_selected_directory() OVERRIDE;
  virtual void set_last_selected_directory(const FilePath& path) OVERRIDE;
#if defined(OS_CHROMEOS)
  virtual void SetupChromeOSEnterpriseExtensionObserver() OVERRIDE {
  }
  virtual void InitChromeOSPreferences() OVERRIDE {
  }
  virtual void ChangeAppLocale(const std::string&,
                               AppLocaleChangedVia) OVERRIDE {
  }
  virtual void OnLogin() OVERRIDE {
  }
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() OVERRIDE;

  // Schedules a task on the history backend and runs a nested loop until the
  // task is processed.  This has the effect of blocking the caller until the
  // history service processes all pending requests.
  void BlockUntilHistoryProcessesPendingRequests();

  // Creates and initializes a profile sync service if the tests require one.
  virtual TokenService* GetTokenService() OVERRIDE;
  virtual ProfileSyncService* GetProfileSyncService() OVERRIDE;
  virtual ProfileSyncService* GetProfileSyncService(
      const std::string& cros_notes) OVERRIDE;
  virtual ChromeBlobStorageContext* GetBlobStorageContext() OVERRIDE;
  virtual ExtensionInfoMap* GetExtensionInfoMap() OVERRIDE;
  virtual PromoCounter* GetInstantPromoCounter() OVERRIDE;
  virtual ChromeURLDataManager* GetChromeURLDataManager() OVERRIDE;
  virtual chrome_browser_net::Predictor* GetNetworkPredictor() OVERRIDE;
  virtual void ClearNetworkingHistorySince(base::Time time) OVERRIDE;
  virtual GURL GetHomePage() OVERRIDE;
  virtual NetworkActionPredictor* GetNetworkActionPredictor() OVERRIDE;

  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE;

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

  scoped_ptr<ExtensionProcessManager> extension_process_manager_;

  scoped_ptr<ExtensionPrefValueMap> extension_pref_value_map_;

  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;

  // The proxy prefs tracker.
  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  // We use a temporary directory to store testing profile data. In a multi-
  // profile environment, this is invalid and the directory is managed by the
  // TestingProfileManager.
  ScopedTempDir temp_dir_;
  // The path to this profile. This will be valid in either of the two above
  // cases.
  FilePath profile_path_;

  scoped_ptr<ChromeURLDataManager> chrome_url_data_manager_;

  // We keep a weak pointer to the dependency manager we want to notify on our
  // death. Defaults to the Singleton implementation but overridable for
  // testing.
  ProfileDependencyManager* profile_dependency_manager_;

  scoped_refptr<ChromeAppCacheService> appcache_service_;

  // The QuotaManager, only available if set explicitly via SetQuotaManager.
  scoped_refptr<quota::QuotaManager> quota_manager_;
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_H_
