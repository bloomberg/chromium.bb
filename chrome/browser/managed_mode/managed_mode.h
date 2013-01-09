// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
template<typename T>
struct DefaultSingletonTraits;
class ManagedModeSiteList;
class ManagedModeURLFilter;
class PrefChangeRegistrar;
class PrefServiceSimple;
class PrefServiceSyncable;
class Profile;

namespace policy{
class URLBlacklist;
}

// Managed mode allows one person to manage the Chrome experience for another
// person by pre-configuring and then locking a managed User profile.
// The ManagedMode class provides methods to check whether the browser is in
// managed mode, and to attempt to enter or leave managed mode.
// Except where otherwise noted, this class should be used on the UI thread.
class ManagedMode : public chrome::BrowserListObserver,
                    public extensions::ManagementPolicy::Provider,
                    public content::NotificationObserver {
 public:
  typedef base::Callback<void(bool)> EnterCallback;

  static void RegisterPrefs(PrefServiceSimple* prefs);
  static void RegisterUserPrefs(PrefServiceSyncable* prefs);

  // Initializes the singleton, setting the managed_profile_. Must be called
  // after g_browser_process and the LocalState have been created.
  static void Init(Profile* profile);
  static bool IsInManagedMode();

  // Calls |callback| with the argument true iff managed mode was entered
  // sucessfully.
  static void EnterManagedMode(Profile* profile, const EnterCallback& callback);
  static void LeaveManagedMode();

  // Returns the URL filter for the IO thread, for filtering network requests
  // (in ChromeNetworkDelegate).
  // This method should only be called on the IO thread.
  static const ManagedModeURLFilter* GetURLFilterForIOThread();

  // Returns the URL filter for the UI thread, for filtering navigations and
  // classifying sites in the history view.
  // This method should only be called on the UI thread.
  static const ManagedModeURLFilter* GetURLFilterForUIThread();

  // The functions that handle manual whitelists use |url_pattern| or lists
  // of "url patterns". An "url pattern" is a pattern in the format used by the
  // policy::URLBlacklist filter. A description of the format used can be found
  // here: http://dev.chromium.org/administrators/url-blacklist-filter-format.
  // They all receive the |is_whitelist| parameter which dictates whether they
  // act on the whitelist (for |is_whitelist| == true) or on the blacklist (for
  // |is_whitelist| == false).

  // Checks if the |url_pattern| is in the manual whitelist.
  static bool IsInManualList(const bool is_whitelist,
                             const std::string& url_pattern);

  // Appends |list| to the manual white/black list (according to |is_whitelist|)
  // both in URL filter and in preferences.
  static void AddToManualList(const bool is_whitelist,
                              const base::ListValue& list);

  // Removes |list| from the manual white/black list (according to
  // |is_whitelist|) both in URL filter and in preferences.
  static void RemoveFromManualList(const bool is_whitelist,
                                   const base::ListValue& list);

  // Updates the whitelist and the blacklist from the prefs.
  static void UpdateManualLists();

  // Returns the profile blacklist.
  static scoped_ptr<base::ListValue> GetBlacklist();

  // ExtensionManagementPolicy::Provider implementation:
  virtual std::string GetDebugPolicyProviderName() const OVERRIDE;
  virtual bool UserMayLoad(const extensions::Extension* extension,
                           string16* error) const OVERRIDE;
  virtual bool UserMayModifySettings(const extensions::Extension* extension,
                                     string16* error) const OVERRIDE;

  // chrome::BrowserListObserver implementation:
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  ManagedMode();
  virtual ~ManagedMode();
  void EnterManagedModeImpl(Profile* profile, const EnterCallback& callback);

  // The managed profile. This is NULL iff we are not in managed mode.
  Profile* managed_profile_;

 private:
  class URLFilterContext;

  friend class Singleton<ManagedMode, LeakySingletonTraits<ManagedMode> >;
  friend struct DefaultSingletonTraits<ManagedMode>;
  FRIEND_TEST_ALL_PREFIXES(ExtensionApiTest, ManagedModeOnChange);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           ManagedModeProhibitsModification);

  static ManagedMode* GetInstance();

  virtual void InitImpl(Profile* profile);

  // Internal implementation for ExtensionManagementPolicy::Delegate methods.
  // If |error| is not NULL, it will be filled with an error message if the
  // requested extension action (install, modify status, etc.) is not permitted.
  bool ExtensionManagementPolicyImpl(string16* error) const;

  void LeaveManagedModeImpl();

  const ManagedModeURLFilter* GetURLFilterForIOThreadImpl();
  const ManagedModeURLFilter* GetURLFilterForUIThreadImpl();

  void FinalizeEnter(bool result);

  // Platform-specific methods that confirm whether we can enter or leave
  // managed mode.
  virtual bool PlatformConfirmEnter();
  virtual bool PlatformConfirmLeave();

  virtual bool IsInManagedModeImpl() const;

  // Enables or disables managed mode and registers or unregisters it with the
  // ManagementPolicy. If |newly_managed_profile| is NULL, managed mode will
  // be disabled. Otherwise, managed mode will be enabled for that profile
  // (typically |managed_profile_|, but other values are possible during
  // testing).
  virtual void SetInManagedMode(Profile* newly_managed_profile);

  // Returns a list of all installed and enabled site lists in the current
  // managed profile.
  // This method should only be called if managed mode is active.
  ScopedVector<ManagedModeSiteList> GetActiveSiteLists();

  void OnDefaultFilteringBehaviorChanged();

  void UpdateManualListsImpl();

  // Returns a copy of the manual whitelist which is stored in each profile.
  scoped_ptr<base::ListValue> GetWhitelist();

  // The following functions use |is_whitelist| to select between the whitelist
  // and the blacklist as the target of the function. If |is_whitelist| is true
  // |url_pattern| is added to the whitelist, otherwise it is added to the
  // blacklist.

  void RemoveFromManualListImpl(const bool is_whitelist,
                                const base::ListValue& whitelist);

  // Adds the |url_pattern| to the manual lists in the URL filter. This is used
  // by AddToManualListImpl().
  void AddURLPatternToManualList(const bool is_whitelist,
                                 const std::string& url_pattern);

  void AddToManualListImpl(const bool is_whitelist,
                           const base::ListValue& whitelist);

  bool IsInManualListImpl(const bool is_whitelist,
                          const std::string& url_pattern);

  content::NotificationRegistrar registrar_;
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;

  scoped_ptr<URLFilterContext> io_url_filter_context_;
  scoped_ptr<URLFilterContext> ui_url_filter_context_;

  std::set<Browser*> browsers_to_close_;
  std::vector<EnterCallback> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ManagedMode);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_H_
