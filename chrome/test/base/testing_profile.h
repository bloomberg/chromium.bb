// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PROFILE_H_
#define CHROME_TEST_BASE_TESTING_PROFILE_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class MockResourceContext;
}

namespace history {
class TopSites;
}

namespace net {
class CookieMonster;
class URLRequestContextGetter;
}

namespace policy {
class PolicyService;
class ProfilePolicyConnector;
class SchemaRegistryService;
}

namespace quota {
class SpecialStoragePolicy;
}

class BrowserContextDependencyManager;
class ExtensionSpecialStoragePolicy;
class HostContentSettingsMap;
class PrefServiceSyncable;
class TestingPrefServiceSyncable;

class TestingProfile : public Profile {
 public:
  // Profile directory name for the test user. This is "Default" on most
  // platforms but must be different on ChromeOS because a logged-in user cannot
  // use "Default" as profile directory.
  // Browser- and UI tests should always use this to get to the user's profile
  // directory. Unit-tests, though, should use |kInitialProfile|, which is
  // always "Default", because they are runnining without logged-in user.
  static const char kTestUserProfileDir[];

  // Default constructor that cannot be used with multi-profiles.
  TestingProfile();

  typedef std::vector<std::pair<
              BrowserContextKeyedServiceFactory*,
              BrowserContextKeyedServiceFactory::TestingFactoryFunction> >
      TestingFactories;

  // Helper class for building an instance of TestingProfile (allows injecting
  // mocks for various services prior to profile initialization).
  // TODO(atwilson): Remove non-default constructors and various setters in
  // favor of using the Builder API.
  class Builder {
   public:
    Builder();
    ~Builder();

    // Sets a Delegate to be called back during profile init. This causes the
    // final initialization to be performed via a task so the caller must run
    // a MessageLoop. Caller maintains ownership of the Delegate
    // and must manage its lifetime so it continues to exist until profile
    // initialization is complete.
    void SetDelegate(Delegate* delegate);

    // Adds a testing factory to the TestingProfile. These testing factories
    // are applied before the ProfileKeyedServices are created.
    void AddTestingFactory(
        BrowserContextKeyedServiceFactory* service_factory,
        BrowserContextKeyedServiceFactory::TestingFactoryFunction callback);

    // Sets the ExtensionSpecialStoragePolicy to be returned by
    // GetExtensionSpecialStoragePolicy().
    void SetExtensionSpecialStoragePolicy(
        scoped_refptr<ExtensionSpecialStoragePolicy> policy);

    // Sets the path to the directory to be used to hold profile data.
    void SetPath(const base::FilePath& path);

    // Sets the PrefService to be used by this profile.
    void SetPrefService(scoped_ptr<PrefServiceSyncable> prefs);

    // Makes the Profile being built an incognito profile.
    void SetIncognito();

    // Makes the Profile being built a guest profile.
    void SetGuestSession();

    // Sets the supervised user ID (which is empty by default). If it is set to
    // a non-empty string, the profile is supervised.
    void SetSupervisedUserId(const std::string& supervised_user_id);

    // Sets the PolicyService to be used by this profile.
    void SetPolicyService(scoped_ptr<policy::PolicyService> policy_service);

    // Creates the TestingProfile using previously-set settings.
    scoped_ptr<TestingProfile> Build();

   private:
    // If true, Build() has already been called.
    bool build_called_;

    // Various staging variables where values are held until Build() is invoked.
    scoped_ptr<PrefServiceSyncable> pref_service_;
    scoped_refptr<ExtensionSpecialStoragePolicy> extension_policy_;
    base::FilePath path_;
    Delegate* delegate_;
    bool incognito_;
    bool guest_session_;
    std::string supervised_user_id_;
    scoped_ptr<policy::PolicyService> policy_service_;
    TestingFactories testing_factories_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  // Multi-profile aware constructor that takes the path to a directory managed
  // for this profile. This constructor is meant to be used by
  // TestingProfileManager::CreateTestingProfile. If you need to create multi-
  // profile profiles, use that factory method instead of this directly.
  // Exception: if you need to create multi-profile profiles for testing the
  // ProfileManager, then use the constructor below instead.
  explicit TestingProfile(const base::FilePath& path);

  // Multi-profile aware constructor that takes the path to a directory managed
  // for this profile and a delegate. This constructor is meant to be used
  // for unittesting the ProfileManager.
  TestingProfile(const base::FilePath& path, Delegate* delegate);

  // Full constructor allowing the setting of all possible instance data.
  // Callers should use Builder::Build() instead of invoking this constructor.
  TestingProfile(const base::FilePath& path,
                 Delegate* delegate,
                 scoped_refptr<ExtensionSpecialStoragePolicy> extension_policy,
                 scoped_ptr<PrefServiceSyncable> prefs,
                 bool incognito,
                 bool guest_session,
                 const std::string& supervised_user_id,
                 scoped_ptr<policy::PolicyService> policy_service,
                 const TestingFactories& factories);

  virtual ~TestingProfile();

  // Creates the favicon service. Consequent calls would recreate the service.
  void CreateFaviconService();

  // Creates the history service. If |delete_file| is true, the history file is
  // deleted first, then the HistoryService is created. As TestingProfile
  // deletes the directory containing the files used by HistoryService, this
  // only matters if you're recreating the HistoryService.  If |no_db| is true,
  // the history backend will fail to initialize its database; this is useful
  // for testing error conditions. Returns true on success.
  bool CreateHistoryService(bool delete_file, bool no_db) WARN_UNUSED_RESULT;

  // Shuts down and nulls out the reference to HistoryService.
  void DestroyHistoryService();

  // Creates TopSites. This returns immediately, and top sites may not be
  // loaded. Use BlockUntilTopSitesLoaded to ensure TopSites has finished
  // loading.
  void CreateTopSites();

  // Shuts down and nulls out the reference to TopSites.
  void DestroyTopSites();

  // Creates the BookmarkBarModel. If not invoked the bookmark bar model is
  // NULL. If |delete_file| is true, the bookmarks file is deleted first, then
  // the model is created. As TestingProfile deletes the directory containing
  // the files used by HistoryService, the boolean only matters if you're
  // recreating the BookmarkModel.
  //
  // NOTE: this does not block until the bookmarks are loaded. For that use
  // WaitForBookmarkModelToLoad().
  void CreateBookmarkModel(bool delete_file);

  // Creates a WebDataService. If not invoked, the web data service is NULL.
  void CreateWebDataService();

  // Blocks until the HistoryService finishes restoring its in-memory cache.
  // This is NOT invoked from CreateHistoryService.
  void BlockUntilHistoryIndexIsRefreshed();

  // Blocks until TopSites finishes loading.
  void BlockUntilTopSitesLoaded();

  // Allow setting a profile as Guest after-the-fact to simplify some tests.
  void SetGuestSession(bool guest);

  TestingPrefServiceSyncable* GetTestingPrefService();

  // content::BrowserContext
  virtual base::FilePath GetPath() const OVERRIDE;
  virtual scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::BrowserPluginGuestManager* GetGuestManager() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;
  virtual content::PushMessagingService* GetPushMessagingService() OVERRIDE;

  virtual TestingProfile* AsTestingProfile() OVERRIDE;

  // Profile
  virtual std::string GetProfileName() OVERRIDE;
  virtual ProfileType GetProfileType() const OVERRIDE;

  // DEPRECATED, because it's fragile to change a profile from non-incognito
  // to incognito after the ProfileKeyedServices have been created (some
  // ProfileKeyedServices either should not exist in incognito mode, or will
  // crash when they try to get references to other services they depend on,
  // but do not exist in incognito mode).
  // TODO(atwilson): Remove this API (http://crbug.com/277296).
  //
  // Changes a profile's to/from incognito mode temporarily - profile will be
  // returned to non-incognito before destruction to allow services to
  // properly shutdown. This is only supported for legacy tests - new tests
  // should create a true incognito profile using Builder::SetIncognito() or
  // by using the TestingProfile constructor that allows setting the incognito
  // flag.
  void ForceIncognito(bool force_incognito) {
    force_incognito_ = force_incognito;
  }

  // Assumes ownership.
  virtual void SetOffTheRecordProfile(scoped_ptr<Profile> profile);
  virtual void SetOriginalProfile(Profile* profile);
  virtual Profile* GetOffTheRecordProfile() OVERRIDE;
  virtual void DestroyOffTheRecordProfile() OVERRIDE {}
  virtual bool HasOffTheRecordProfile() OVERRIDE;
  virtual Profile* GetOriginalProfile() OVERRIDE;
  virtual bool IsSupervised() OVERRIDE;
  void SetExtensionSpecialStoragePolicy(
      ExtensionSpecialStoragePolicy* extension_special_storage_policy);
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() OVERRIDE;
  // TODO(ajwong): Remove this API in favor of directly retrieving the
  // CookieStore from the StoragePartition after ExtensionURLRequestContext
  // has been removed.
  net::CookieMonster* GetCookieMonster();

  virtual PrefService* GetPrefs() OVERRIDE;

  virtual history::TopSites* GetTopSites() OVERRIDE;
  virtual history::TopSites* GetTopSitesWithoutCreating() OVERRIDE;

  virtual net::URLRequestContextGetter* GetMediaRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual net::SSLConfigService* GetSSLConfigService() OVERRIDE;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() OVERRIDE;
  void set_last_session_exited_cleanly(bool value) {
    last_session_exited_cleanly_ = value;
  }
  virtual bool IsSameProfile(Profile *p) OVERRIDE;
  virtual base::Time GetStartTime() const OVERRIDE;
  virtual base::FilePath last_selected_directory() OVERRIDE;
  virtual void set_last_selected_directory(const base::FilePath& path) OVERRIDE;
  virtual bool WasCreatedByVersionOrLater(const std::string& version) OVERRIDE;
  virtual bool IsGuestSession() const OVERRIDE;
  virtual void SetExitType(ExitType exit_type) OVERRIDE {}
  virtual ExitType GetLastSessionExitType() OVERRIDE;
#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string&,
                               AppLocaleChangedVia) OVERRIDE {
  }
  virtual void OnLogin() OVERRIDE {
  }
  virtual void InitChromeOSPreferences() OVERRIDE {
  }
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() OVERRIDE;

  // Schedules a task on the history backend and runs a nested loop until the
  // task is processed.  This has the effect of blocking the caller until the
  // history service processes all pending requests.
  void BlockUntilHistoryProcessesPendingRequests();

  virtual chrome_browser_net::Predictor* GetNetworkPredictor() OVERRIDE;
  virtual DevToolsNetworkController* GetDevToolsNetworkController() OVERRIDE;
  virtual void ClearNetworkingHistorySince(
      base::Time time,
      const base::Closure& completion) OVERRIDE;
  virtual GURL GetHomePage() OVERRIDE;

  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE;

  void set_profile_name(const std::string& profile_name) {
    profile_name_ = profile_name;
  }

 protected:
  base::Time start_time_;
  scoped_ptr<PrefServiceSyncable> prefs_;
  // ref only for right type, lifecycle is managed by prefs_
  TestingPrefServiceSyncable* testing_prefs_;

 private:
  // Creates a temporary directory for use by this profile.
  void CreateTempProfileDir();

  // Common initialization between the two constructors.
  void Init();

  // Finishes initialization when a profile is created asynchronously.
  void FinishInit();

  // Creates a TestingPrefService and associates it with the TestingProfile.
  void CreateTestingPrefService();

  // Creates a ProfilePolicyConnector that the ProfilePolicyConnectorFactory
  // maps to this profile.
  void CreateProfilePolicyConnector();

  // Internally, this is a TestURLRequestContextGetter that creates a dummy
  // request context. Currently, only the CookieMonster is hooked up.
  scoped_refptr<net::URLRequestContextGetter> extensions_request_context_;

  bool incognito_;
  bool force_incognito_;
  scoped_ptr<Profile> incognito_profile_;
  Profile* original_profile_;

  bool guest_session_;

  std::string supervised_user_id_;

  // Did the last session exit cleanly? Default is true.
  bool last_session_exited_cleanly_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  base::FilePath last_selected_directory_;
  scoped_refptr<history::TopSites> top_sites_;  // For history and thumbnails.

  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;

  // The proxy prefs tracker.
  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  // We use a temporary directory to store testing profile data. In a multi-
  // profile environment, this is invalid and the directory is managed by the
  // TestingProfileManager.
  base::ScopedTempDir temp_dir_;
  // The path to this profile. This will be valid in either of the two above
  // cases.
  base::FilePath profile_path_;

  // We keep a weak pointer to the dependency manager we want to notify on our
  // death. Defaults to the Singleton implementation but overridable for
  // testing.
  BrowserContextDependencyManager* browser_context_dependency_manager_;

  // Owned, but must be deleted on the IO thread so not placing in a
  // scoped_ptr<>.
  content::MockResourceContext* resource_context_;

#if defined(ENABLE_CONFIGURATION_POLICY)
  scoped_ptr<policy::SchemaRegistryService> schema_registry_service_;
#endif
  scoped_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  // Weak pointer to a delegate for indicating that a profile was created.
  Delegate* delegate_;

  std::string profile_name_;

  scoped_ptr<policy::PolicyService> policy_service_;
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_H_
