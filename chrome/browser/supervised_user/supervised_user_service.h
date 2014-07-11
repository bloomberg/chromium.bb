// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/management_policy.h"
#endif

class Browser;
class GoogleServiceAuthError;
class PermissionRequestCreator;
class Profile;
class SupervisedUserRegistrationUtility;
class SupervisedUserSettingsService;
class SupervisedUserSiteList;
class SupervisedUserURLFilter;

namespace extensions {
class ExtensionRegistry;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This class handles all the information related to a given supervised profile
// (e.g. the installed content packs, the default URL filtering behavior, or
// manual whitelist/blacklist overrides).
class SupervisedUserService : public KeyedService,
#if defined(ENABLE_EXTENSIONS)
                              public extensions::ManagementPolicy::Provider,
                              public extensions::ExtensionRegistryObserver,
#endif
                              public ProfileSyncServiceObserver,
                              public chrome::BrowserListObserver {
 public:
  typedef std::vector<base::string16> CategoryList;
  typedef base::Callback<void(content::WebContents*)> NavigationBlockedCallback;
  typedef base::Callback<void(const GoogleServiceAuthError&)> AuthErrorCallback;

  enum ManualBehavior {
    MANUAL_NONE = 0,
    MANUAL_ALLOW,
    MANUAL_BLOCK
  };

  class Delegate {
   public:
    virtual ~Delegate() {}
    // Returns true to indicate that the delegate handled the (de)activation, or
    // false to indicate that the SupervisedUserService itself should handle it.
    virtual bool SetActive(bool active) = 0;
  };

  virtual ~SupervisedUserService();

  // ProfileKeyedService override:
  virtual void Shutdown() OVERRIDE;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void SetDelegate(Delegate* delegate);

  // Returns the URL filter for the IO thread, for filtering network requests
  // (in SupervisedUserResourceThrottle).
  scoped_refptr<const SupervisedUserURLFilter> GetURLFilterForIOThread();

  // Returns the URL filter for the UI thread, for filtering navigations and
  // classifying sites in the history view.
  SupervisedUserURLFilter* GetURLFilterForUIThread();

  // Returns the URL's category, obtained from the installed content packs.
  int GetCategory(const GURL& url);

  // Returns the list of all known human-readable category names, sorted by ID
  // number. Called in the critical path of drawing the history UI, so needs to
  // be fast.
  void GetCategoryNames(CategoryList* list);

  // Whether the user can request access to blocked URLs.
  bool AccessRequestsEnabled();

  void OnPermissionRequestIssued();

  // Adds an access request for the given URL. The requests are stored using
  // a prefix followed by a URIEncoded version of the URL. Each entry contains
  // a dictionary which currently has the timestamp of the request in it.
  void AddAccessRequest(const GURL& url);

  // Returns the email address of the custodian.
  std::string GetCustodianEmailAddress() const;

  // Returns the name of the custodian, or the email address if the name is
  // empty.
  std::string GetCustodianName() const;

  // These methods allow querying and modifying the manual filtering behavior.
  // The manual behavior is set by the user and overrides all other settings
  // (whitelists or the default behavior).

  // Returns the manual behavior for the given host.
  ManualBehavior GetManualBehaviorForHost(const std::string& hostname);

  // Returns the manual behavior for the given URL.
  ManualBehavior GetManualBehaviorForURL(const GURL& url);

  // Returns all URLS on the given host that have exceptions.
  void GetManualExceptionsForHost(const std::string& host,
                                  std::vector<GURL>* urls);

  // Initializes this object. This method does nothing if the profile is not
  // supervised.
  void Init();

  // Initializes this profile for syncing, using the provided |refresh_token| to
  // mint access tokens for Sync.
  void InitSync(const std::string& refresh_token);

  // Convenience method that registers this supervised user using
  // |registration_utility| and initializes sync with the returned token.
  // The |callback| will be called when registration is complete,
  // whether it suceeded or not -- unless registration was cancelled manually,
  // in which case the callback will be ignored.
  void RegisterAndInitSync(
      SupervisedUserRegistrationUtility* registration_utility,
      Profile* custodian_profile,
      const std::string& supervised_user_id,
      const AuthErrorCallback& callback);

  void set_elevated_for_testing(bool skip) {
    elevated_for_testing_ = skip;
  }

  void AddNavigationBlockedCallback(const NavigationBlockedCallback& callback);
  void DidBlockNavigation(content::WebContents* web_contents);

#if defined(ENABLE_EXTENSIONS)
  // extensions::ManagementPolicy::Provider implementation:
  virtual std::string GetDebugPolicyProviderName() const OVERRIDE;
  virtual bool UserMayLoad(const extensions::Extension* extension,
                           base::string16* error) const OVERRIDE;
  virtual bool UserMayModifySettings(const extensions::Extension* extension,
                                     base::string16* error) const OVERRIDE;

  // extensions::ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;
#endif

  // ProfileSyncServiceObserver implementation:
  virtual void OnStateChanged() OVERRIDE;

  // chrome::BrowserListObserver implementation:
  virtual void OnBrowserSetLastActive(Browser* browser) OVERRIDE;

 private:
  friend class SupervisedUserServiceExtensionTestBase;
  friend class SupervisedUserServiceFactory;
  FRIEND_TEST_ALL_PREFIXES(SupervisedUserServiceTest, ClearOmitOnRegistration);

  // A bridge from the UI thread to the SupervisedUserURLFilters, one of which
  // lives on the IO thread. This class mediates access to them and makes sure
  // they are kept in sync.
  class URLFilterContext {
   public:
    URLFilterContext();
    ~URLFilterContext();

    SupervisedUserURLFilter* ui_url_filter() const;
    SupervisedUserURLFilter* io_url_filter() const;

    void SetDefaultFilteringBehavior(
        SupervisedUserURLFilter::FilteringBehavior behavior);
    void LoadWhitelists(ScopedVector<SupervisedUserSiteList> site_lists);
    void SetManualHosts(scoped_ptr<std::map<std::string, bool> > host_map);
    void SetManualURLs(scoped_ptr<std::map<GURL, bool> > url_map);

   private:
    // SupervisedUserURLFilter is refcounted because the IO thread filter is
    // used both by ProfileImplIOData and OffTheRecordProfileIOData (to filter
    // network requests), so they both keep a reference to it.
    // Clients should not keep references to the UI thread filter, however
    // (the filter will live as long as the profile lives, and afterwards it
    // should not be used anymore either).
    scoped_refptr<SupervisedUserURLFilter> ui_url_filter_;
    scoped_refptr<SupervisedUserURLFilter> io_url_filter_;

    DISALLOW_COPY_AND_ASSIGN(URLFilterContext);
  };

  // Use |SupervisedUserServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  explicit SupervisedUserService(Profile* profile);

  void SetActive(bool active);

  void OnCustodianProfileDownloaded(const base::string16& full_name);

  void OnSupervisedUserRegistered(const AuthErrorCallback& callback,
                                  Profile* custodian_profile,
                                  const GoogleServiceAuthError& auth_error,
                                  const std::string& token);

  void SetupSync();
  void StartSetupSync();
  void FinishSetupSyncWhenReady();
  void FinishSetupSync();

  bool ProfileIsSupervised() const;

#if defined(ENABLE_EXTENSIONS)
  // Internal implementation for ExtensionManagementPolicy::Delegate methods.
  // If |error| is not NULL, it will be filled with an error message if the
  // requested extension action (install, modify status, etc.) is not permitted.
  bool ExtensionManagementPolicyImpl(const extensions::Extension* extension,
                                     base::string16* error) const;

  // Returns a list of all installed and enabled site lists in the current
  // supervised profile.
  ScopedVector<SupervisedUserSiteList> GetActiveSiteLists();

  // Extensions helper to SetActive().
  void SetExtensionsActive();
#endif

  SupervisedUserSettingsService* GetSettingsService();

  void OnSupervisedUserIdChanged();

  void OnDefaultFilteringBehaviorChanged();

  void UpdateSiteLists();

  // Updates the manual overrides for hosts in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualHosts();

  // Updates the manual overrides for URLs in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualURLs();

  // Owns us via the KeyedService mechanism.
  Profile* profile_;

  bool active_;

  Delegate* delegate_;

#if defined(ENABLE_EXTENSIONS)
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;
#endif

  PrefChangeRegistrar pref_change_registrar_;

  // True iff we're waiting for the Sync service to be initialized.
  bool waiting_for_sync_initialization_;
  bool is_profile_active_;

  std::vector<NavigationBlockedCallback> navigation_blocked_callbacks_;

  // Sets a profile in elevated state for testing if set to true.
  bool elevated_for_testing_;

  // True only when |Shutdown()| method has been called.
  bool did_shutdown_;

  URLFilterContext url_filter_context_;

  // Used to create permission requests.
  scoped_ptr<PermissionRequestCreator> permissions_creator_;

  // True iff we are waiting for a permission request to be issued.
  bool waiting_for_permissions_;

  base::WeakPtrFactory<SupervisedUserService> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_H_
