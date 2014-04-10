// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/strings/string16.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/management_policy.h"

class Browser;
class GoogleServiceAuthError;
class ManagedModeURLFilter;
class ManagedModeSiteList;
class ManagedUserRegistrationUtility;
class ManagedUserSettingsService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// This class handles all the information related to a given managed profile
// (e.g. the installed content packs, the default URL filtering behavior, or
// manual whitelist/blacklist overrides).
class ManagedUserService : public KeyedService,
                           public extensions::ManagementPolicy::Provider,
                           public ProfileSyncServiceObserver,
                           public content::NotificationObserver,
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

  virtual ~ManagedUserService();

  // ProfileKeyedService override:
  virtual void Shutdown() OVERRIDE;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void MigrateUserPrefs(PrefService* prefs);

  // Returns the URL filter for the IO thread, for filtering network requests
  // (in ManagedModeResourceThrottle).
  scoped_refptr<const ManagedModeURLFilter> GetURLFilterForIOThread();

  // Returns the URL filter for the UI thread, for filtering navigations and
  // classifying sites in the history view.
  ManagedModeURLFilter* GetURLFilterForUIThread();

  // Returns the URL's category, obtained from the installed content packs.
  int GetCategory(const GURL& url);

  // Returns the list of all known human-readable category names, sorted by ID
  // number. Called in the critical path of drawing the history UI, so needs to
  // be fast.
  void GetCategoryNames(CategoryList* list);

  // Whether the user can request access to blocked URLs.
  bool AccessRequestsEnabled();

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
  // managed.
  void Init();

  // Initializes this profile for syncing, using the provided |refresh_token| to
  // mint access tokens for Sync.
  void InitSync(const std::string& refresh_token);

  // Convenience method that registers this managed user using
  // |registration_utility| and initializes sync with the returned token.
  // The |callback| will be called when registration is complete,
  // whether it suceeded or not -- unless registration was cancelled manually,
  // in which case the callback will be ignored.
  void RegisterAndInitSync(ManagedUserRegistrationUtility* registration_utility,
                           Profile* custodian_profile,
                           const std::string& managed_user_id,
                           const AuthErrorCallback& callback);

  void set_elevated_for_testing(bool skip) {
    elevated_for_testing_ = skip;
  }

  void AddNavigationBlockedCallback(const NavigationBlockedCallback& callback);
  void DidBlockNavigation(content::WebContents* web_contents);

  // extensions::ManagementPolicy::Provider implementation:
  virtual std::string GetDebugPolicyProviderName() const OVERRIDE;
  virtual bool UserMayLoad(const extensions::Extension* extension,
                           base::string16* error) const OVERRIDE;
  virtual bool UserMayModifySettings(const extensions::Extension* extension,
                                     base::string16* error) const OVERRIDE;

  // ProfileSyncServiceObserver implementation:
  virtual void OnStateChanged() OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // chrome::BrowserListObserver implementation:
  virtual void OnBrowserSetLastActive(Browser* browser) OVERRIDE;

 private:
  friend class ManagedUserServiceExtensionTestBase;
  friend class ManagedUserServiceFactory;
  FRIEND_TEST_ALL_PREFIXES(ManagedUserServiceTest, ClearOmitOnRegistration);

  // A bridge from ManagedMode (which lives on the UI thread) to the
  // ManagedModeURLFilters, one of which lives on the IO thread. This class
  // mediates access to them and makes sure they are kept in sync.
  class URLFilterContext {
   public:
    URLFilterContext();
    ~URLFilterContext();

    ManagedModeURLFilter* ui_url_filter() const;
    ManagedModeURLFilter* io_url_filter() const;

    void SetDefaultFilteringBehavior(
        ManagedModeURLFilter::FilteringBehavior behavior);
    void LoadWhitelists(ScopedVector<ManagedModeSiteList> site_lists);
    void SetManualHosts(scoped_ptr<std::map<std::string, bool> > host_map);
    void SetManualURLs(scoped_ptr<std::map<GURL, bool> > url_map);

   private:
    // ManagedModeURLFilter is refcounted because the IO thread filter is used
    // both by ProfileImplIOData and OffTheRecordProfileIOData (to filter
    // network requests), so they both keep a reference to it.
    // Clients should not keep references to the UI thread filter, however
    // (the filter will live as long as the profile lives, and afterwards it
    // should not be used anymore either).
    scoped_refptr<ManagedModeURLFilter> ui_url_filter_;
    scoped_refptr<ManagedModeURLFilter> io_url_filter_;

    DISALLOW_COPY_AND_ASSIGN(URLFilterContext);
  };

  // Use |ManagedUserServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  explicit ManagedUserService(Profile* profile);

  void OnCustodianProfileDownloaded(const base::string16& full_name);

  void OnManagedUserRegistered(const AuthErrorCallback& callback,
                               Profile* custodian_profile,
                               const GoogleServiceAuthError& auth_error,
                               const std::string& token);

  void SetupSync();

  bool ProfileIsManaged() const;

  // Internal implementation for ExtensionManagementPolicy::Delegate methods.
  // If |error| is not NULL, it will be filled with an error message if the
  // requested extension action (install, modify status, etc.) is not permitted.
  bool ExtensionManagementPolicyImpl(const extensions::Extension* extension,
                                     base::string16* error) const;

  // Returns a list of all installed and enabled site lists in the current
  // managed profile.
  ScopedVector<ManagedModeSiteList> GetActiveSiteLists();

  ManagedUserSettingsService* GetSettingsService();

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

  content::NotificationRegistrar registrar_;
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

  base::WeakPtrFactory<ManagedUserService> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_H_
