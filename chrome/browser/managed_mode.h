// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_H_
#define CHROME_BROWSER_MANAGED_MODE_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
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
                    public content::NotificationObserver {
 public:
  typedef base::Callback<void(bool)> EnterCallback;

  static void RegisterPrefs(PrefService* prefs);
  static bool IsInManagedMode();

  // Calls |callback| with the argument true iff managed mode was entered
  // sucessfully.
  static void EnterManagedMode(Profile* profile, const EnterCallback& callback);
  static void LeaveManagedMode();

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

 private:
  friend struct DefaultSingletonTraits<ManagedMode>;
  friend class Singleton<ManagedMode>;

  static ManagedMode* GetInstance();

  void LeaveManagedModeImpl();

  void FinalizeEnter(bool result);

  // Platform-specific methods that confirm whether we can enter or leave
  // managed mode.
  virtual bool PlatformConfirmEnter();
  virtual bool PlatformConfirmLeave();

  virtual bool IsInManagedModeImpl();
  virtual void SetInManagedMode(bool in_managed_mode);

  content::NotificationRegistrar registrar_;

  // The managed profile. This is non-NULL only while we're entering
  // managed mode.
  const Profile* managed_profile_;
  std::set<Browser*> browsers_to_close_;
  std::vector<EnterCallback> callbacks_;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_H_
