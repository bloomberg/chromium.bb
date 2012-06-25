// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_H_
#define CHROME_BROWSER_MANAGED_MODE_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
template<typename T>
struct DefaultSingletonTraits;
class PrefService;
class Profile;

// Managed mode allows one person to manage the Chrome experience for another
// person by pre-configuring and then locking a managed User profile.
// The ManagedMode class provides methods to check whether the browser is in
// managed mode, and to attempt to enter or leave managed mode.
class ManagedMode : public BrowserList::Observer,
                    public extensions::ManagementPolicy::Provider,
                    public content::NotificationObserver {
 public:
  typedef base::Callback<void(bool)> EnterCallback;

  static void RegisterPrefs(PrefService* prefs);

  // Initializes the singleton, setting the managed_profile_. Must be called
  // after g_browser_process and the LocalState have been created.
  static void Init(Profile* profile);
  static bool IsInManagedMode();

  // Calls |callback| with the argument true iff managed mode was entered
  // sucessfully.
  static void EnterManagedMode(Profile* profile, const EnterCallback& callback);
  static void LeaveManagedMode();

  // ExtensionManagementPolicy::Provider implementation:
  virtual std::string GetDebugPolicyProviderName() const OVERRIDE;
  virtual bool UserMayLoad(const extensions::Extension* extension,
                           string16* error) const OVERRIDE;
  virtual bool UserMayModifySettings(const extensions::Extension* extension,
                                     string16* error) const OVERRIDE;

  // BrowserList::Observer implementation:
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
  friend struct DefaultSingletonTraits<ManagedMode>;
  friend class Singleton<ManagedMode>;
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

  content::NotificationRegistrar registrar_;

  std::set<Browser*> browsers_to_close_;
  std::vector<EnterCallback> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ManagedMode);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_H_
