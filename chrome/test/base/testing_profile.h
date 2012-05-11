// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

namespace content {
class MockResourceContext;
class SpeechRecognitionPreferences;
}

namespace history {
class TopSites;
}

namespace net {
class CookieMonster;
class URLRequestContextGetter;
}

namespace quota {
class SpecialStoragePolicy;
}

class AutocompleteClassifier;
class BookmarkModel;
class CommandLine;
class ExtensionPrefs;
class ExtensionSpecialStoragePolicy;
class FaviconService;
class HistoryService;
class HostContentSettingsMap;
class PrefService;
class ProfileDependencyManager;
class ProfileSyncService;
class TemplateURLService;
class TestingPrefService;

class TestingProfile : public Profile {
 public:
  // Default constructor that cannot be used with multi-profiles.
  TestingProfile();

  // Multi-profile aware constructor that takes the path to a directory managed
  // for this profile. This constructor is meant to be used by
  // TestingProfileManager::CreateTestingProfile. If you need to create multi-
  // profile profiles, use that factory method instead of this directly.
  // Exception: if you need to create multi-profile profiles for testing the
  // ProfileManager, then use the constructor below instead.
  explicit TestingProfile(const FilePath& path);

  // Multi-profile aware constructor that takes the path to a directory managed
  // for this profile and a delegate. This constructor is meant to be used
  // for unittesting the ProfileManager.
  TestingProfile(const FilePath& path, Delegate* delegate);

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

  // Creates a WebDataService. If not invoked, the web data service is NULL.
  void CreateWebDataService();

  // Blocks until the BookmarkModel finishes loaded. This is NOT invoked from
  // CreateBookmarkModel.
  void BlockUntilBookmarkModelLoaded();

  // Blocks until TopSites finishes loading.
  void BlockUntilTopSitesLoaded();

  // Creates a TemplateURLService. If not invoked, the TemplateURLService is
  // NULL.
  void CreateTemplateURLService();

  // Blocks until TempalteURLService finishes loading.
  void BlockUntilTemplateURLServiceLoaded();

  TestingPrefService* GetTestingPrefService();

  // content::BrowserContext
  virtual FilePath GetPath() OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual content::DownloadManager* GetDownloadManager() OVERRIDE;
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
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual content::SpeechRecognitionPreferences*
      GetSpeechRecognitionPreferences() OVERRIDE;
  virtual bool DidLastSessionExitCleanly() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;

  virtual TestingProfile* AsTestingProfile() OVERRIDE;
  virtual std::string GetProfileName() OVERRIDE;
  void set_incognito(bool incognito) { incognito_ = incognito; }
  // Assumes ownership.
  virtual void SetOffTheRecordProfile(Profile* profile);
  virtual Profile* GetOffTheRecordProfile() OVERRIDE;
  virtual void DestroyOffTheRecordProfile() OVERRIDE {}
  virtual GAIAInfoUpdateService* GetGAIAInfoUpdateService() OVERRIDE;
  virtual bool HasOffTheRecordProfile() OVERRIDE;
  virtual Profile* GetOriginalProfile() OVERRIDE;
  virtual VisitedLinkMaster* GetVisitedLinkMaster() OVERRIDE;
  virtual ExtensionService* GetExtensionService() OVERRIDE;
  virtual UserScriptMaster* GetUserScriptMaster() OVERRIDE;
  virtual ExtensionProcessManager* GetExtensionProcessManager() OVERRIDE;
  virtual ExtensionEventRouter* GetExtensionEventRouter() OVERRIDE;
  void SetExtensionSpecialStoragePolicy(
      ExtensionSpecialStoragePolicy* extension_special_storage_policy);
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() OVERRIDE;
  virtual FaviconService* GetFaviconService(ServiceAccessType access) OVERRIDE;
  virtual HistoryService* GetHistoryService(ServiceAccessType access) OVERRIDE;
  virtual HistoryService* GetHistoryServiceWithoutCreating() OVERRIDE;
  // The CookieMonster will only be returned if a Context has been created. Do
  // this by calling CreateRequestContext(). See the note at GetRequestContext
  // for more information.
  net::CookieMonster* GetCookieMonster();
  virtual AutocompleteClassifier* GetAutocompleteClassifier() OVERRIDE;
  virtual history::ShortcutsBackend* GetShortcutsBackend() OVERRIDE;
  // Sets the profile's PrefService. If a pref service hasn't been explicitly
  // set GetPrefs creates one, so normally you need not invoke this. If you need
  // to set a pref service you must invoke this before GetPrefs.
  // TestingPrefService takes ownership of |prefs|.
  void SetPrefService(PrefService* prefs);
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual history::TopSites* GetTopSites() OVERRIDE;
  virtual history::TopSites* GetTopSitesWithoutCreating() OVERRIDE;

  void CreateRequestContext();
  // Clears out the created request context (which must be done before shutting
  // down the IO thread to avoid leaks).
  void ResetRequestContext();

  virtual net::URLRequestContextGetter* GetRequestContextForMedia() OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForIsolatedApp(
      const std::string& app_id) OVERRIDE;
  virtual net::SSLConfigService* GetSSLConfigService() OVERRIDE;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() OVERRIDE;
  virtual std::wstring GetName();
  virtual void SetName(const std::wstring& name) {}
  virtual std::wstring GetID();
  virtual void SetID(const std::wstring& id);
  void set_last_session_exited_cleanly(bool value) {
    last_session_exited_cleanly_ = value;
  }
  virtual void MergeResourceString(int message_id,
                                   std::wstring* output_string) {}
  virtual void MergeResourceInteger(int message_id, int* output_value) {}
  virtual void MergeResourceBoolean(int message_id, bool* output_value) {}
  virtual BookmarkModel* GetBookmarkModel() OVERRIDE;
  virtual bool IsSameProfile(Profile *p) OVERRIDE;
  virtual base::Time GetStartTime() const OVERRIDE;
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry() OVERRIDE;
  virtual void MarkAsCleanShutdown() OVERRIDE {}
  virtual void InitPromoResources() OVERRIDE {}
  virtual void InitRegisteredProtocolHandlers() OVERRIDE {}

  virtual FilePath last_selected_directory() OVERRIDE;
  virtual void set_last_selected_directory(const FilePath& path) OVERRIDE;
  virtual bool WasCreatedByVersionOrLater(const std::string& version) OVERRIDE;
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

  virtual chrome_browser_net::Predictor* GetNetworkPredictor() OVERRIDE;
  virtual void ClearNetworkingHistorySince(base::Time time) OVERRIDE;
  virtual GURL GetHomePage() OVERRIDE;

  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE;

 protected:
  base::Time start_time_;
  scoped_ptr<PrefService> prefs_;
  // ref only for right type, lifecycle is managed by prefs_
  TestingPrefService* testing_prefs_;

 private:
  // Common initialization between the two constructors.
  void Init();

  // Finishes initialization when a profile is created asynchronously.
  void FinishInit();

  // Destroys favicon service if it has been created.
  void DestroyFaviconService();

  // Creates a TestingPrefService and associates it with the TestingProfile.
  void CreateTestingPrefService();

  virtual base::Callback<ChromeURLDataManagerBackend*(void)>
      GetChromeURLDataManagerBackendGetter() const OVERRIDE;

  // The favicon service. Only created if CreateFaviconService is invoked.
  scoped_ptr<FaviconService> favicon_service_;

  // The history service. Only created if CreateHistoryService is invoked.
  scoped_refptr<HistoryService> history_service_;

  // The BookmarkModel. Only created if CreateBookmarkModel is invoked.
  scoped_ptr<BookmarkModel> bookmark_bar_model_;

  // The ProtocolHandlerRegistry. Only created if CreateProtocolHandlerRegistry
  // is invoked.
  scoped_refptr<ProtocolHandlerRegistry> protocol_handler_registry_;

  // The AutocompleteClassifier.  Only created if CreateAutocompleteClassifier
  // is invoked.
  scoped_ptr<AutocompleteClassifier> autocomplete_classifier_;

  // Internally, this is a TestURLRequestContextGetter that creates a dummy
  // request context. Currently, only the CookieMonster is hooked up.
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_refptr<net::URLRequestContextGetter> extensions_request_context_;

  std::wstring id_;

  bool incognito_;
  scoped_ptr<Profile> incognito_profile_;

  // Did the last session exit cleanly? Default is true.
  bool last_session_exited_cleanly_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content::GeolocationPermissionContext>
      geolocation_permission_context_;

  scoped_refptr<content::SpeechRecognitionPreferences>
      speech_recognition_preferences_;

  FilePath last_selected_directory_;
  scoped_refptr<history::TopSites> top_sites_;  // For history and thumbnails.

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

  // We keep a weak pointer to the dependency manager we want to notify on our
  // death. Defaults to the Singleton implementation but overridable for
  // testing.
  ProfileDependencyManager* profile_dependency_manager_;

  scoped_ptr<content::MockResourceContext> resource_context_;

  // Weak pointer to a delegate for indicating that a profile was created.
  Delegate* delegate_;
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_H_
