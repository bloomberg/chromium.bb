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
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
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

namespace policy {
class URLBlacklist;
}

// Managed mode locks the UI to a certain managed user profile, preventing the
// user from accessing other profiles.
// The ManagedMode class provides methods to check whether the browser is in
// managed mode, and to attempt to enter or leave managed mode.
// Except where otherwise noted, this class should be used on the UI thread.
class ManagedMode : public chrome::BrowserListObserver,
                    public content::NotificationObserver {
 public:
  typedef base::Callback<void(bool)> EnterCallback;

  static void RegisterPrefs(PrefServiceSimple* prefs);

  // Initializes the singleton, setting the managed_profile_. Must be called
  // after g_browser_process and the LocalState have been created.
  static void Init(Profile* profile);
  static bool IsInManagedMode();

  // Calls |callback| with the argument true iff managed mode was entered
  // sucessfully.
  static void EnterManagedMode(Profile* profile, const EnterCallback& callback);
  static void LeaveManagedMode();

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
  friend class Singleton<ManagedMode, LeakySingletonTraits<ManagedMode> >;
  friend struct DefaultSingletonTraits<ManagedMode>;
  FRIEND_TEST_ALL_PREFIXES(ExtensionApiTest, ManagedModeOnChange);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           ManagedModeProhibitsModification);
  FRIEND_TEST_ALL_PREFIXES(ManagedModeContentPackTest, InstallContentPacks);

  static ManagedMode* GetInstance();

  virtual void InitImpl(Profile* profile);

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

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_H_
