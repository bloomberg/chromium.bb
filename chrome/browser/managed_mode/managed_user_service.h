// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_H_

#include <vector>

#include "base/prefs/public/pref_change_registrar.h"
#include "base/string16.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ManagedModeURLFilter;
class ManagedModeSiteList;
class PrefServiceSyncable;
class Profile;

// This class handles all the information related to a given managed profile
// (e.g. the installed content packs, the default URL filtering behavior, or
// manual whitelist/blacklist overrides).
class ManagedUserService : public ProfileKeyedService,
                           public extensions::ManagementPolicy::Provider,
                           public content::NotificationObserver {
 public:
  typedef std::vector<string16> CategoryList;

  explicit ManagedUserService(Profile* profile);
  virtual ~ManagedUserService();

  bool ProfileIsManaged() const;

  static void RegisterUserPrefs(PrefServiceSyncable* prefs);

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

  // The functions that handle manual whitelists use |url_pattern| or lists
  // of "url patterns". An "url pattern" is a pattern in the format used by the
  // policy::URLBlacklist filter. A description of the format used can be found
  // here: http://dev.chromium.org/administrators/url-blacklist-filter-format.
  // They all receive the |is_whitelist| parameter which dictates whether they
  // act on the whitelist (for |is_whitelist| == true) or on the blacklist (for
  // |is_whitelist| == false).

  // Checks if the |url_pattern| is in the manual whitelist.
  bool IsInManualList(const bool is_whitelist, const std::string& url_pattern);

  // Appends |list| to the manual white/black list (according to |is_whitelist|)
  // both in URL filter and in preferences.
  void AddToManualList(const bool is_whitelist, const base::ListValue& list);

  // Removes |list| from the manual white/black list (according to
  // |is_whitelist|) both in URL filter and in preferences.
  void RemoveFromManualList(const bool is_whitelist,
                            const base::ListValue& list);

  // Updates the whitelist and the blacklist from the prefs.
  void UpdateManualLists();

  void SetElevatedForTesting(bool is_elevated);

  // Initializes this object. This method does nothing if the profile is not
  // managed. This method should only be called for testing, to do
  // initialization after the profile has been manually set to managed,
  // otherwise it is called automatically,
  void Init();

  // ExtensionManagementPolicy::Provider implementation:
  virtual std::string GetDebugPolicyProviderName() const OVERRIDE;
  virtual bool UserMayLoad(const extensions::Extension* extension,
                           string16* error) const OVERRIDE;
  virtual bool UserMayModifySettings(const extensions::Extension* extension,
                                     string16* error) const OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ManagedUserServiceExtensionTest;

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
    void SetManualLists(scoped_ptr<base::ListValue> whitelist,
                        scoped_ptr<base::ListValue> blacklist);
    void AddURLPatternToManualList(const bool isWhitelist,
                                   const std::string& url);

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

  // Internal implementation for ExtensionManagementPolicy::Delegate methods.
  // If |error| is not NULL, it will be filled with an error message if the
  // requested extension action (install, modify status, etc.) is not permitted.
  bool ExtensionManagementPolicyImpl(string16* error) const;

  // Returns a list of all installed and enabled site lists in the current
  // managed profile.
  ScopedVector<ManagedModeSiteList> GetActiveSiteLists();

  void OnDefaultFilteringBehaviorChanged();

  void UpdateSiteLists();

  // Adds the |url_pattern| to the manual lists in the URL filter. This is used
  // by AddToManualListImpl().
  void AddURLPatternToManualList(const bool is_whitelist,
                                 const std::string& url_pattern);

  // Returns a copy of the manual whitelist which is stored in each profile.
  scoped_ptr<base::ListValue> GetWhitelist();

  // Returns a copy of the manual blacklist which is stored in each profile.
  scoped_ptr<base::ListValue> GetBlacklist();

  // Owns us via the ProfileKeyedService mechanism.
  Profile* profile_;

  // If ManagedUserService is in an elevated state, a custodian user has
  // authorized making changes (to install additional content packs, for
  // example).
  bool is_elevated_;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  URLFilterContext url_filter_context_;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_H_
