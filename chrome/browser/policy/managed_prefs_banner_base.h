// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MANAGED_PREFS_BANNER_BASE_H_
#define CHROME_BROWSER_POLICY_MANAGED_PREFS_BANNER_BASE_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/ui/options/options_window.h"
#include "content/common/notification_observer.h"

class PrefService;
class PrefSetObserver;

namespace policy {

// Common base functionality for the managed prefs warning banner displayed in
// the preference dialogs when there are options that are controlled by
// configuration policy and thus cannot be changed by the user.
class ManagedPrefsBannerBase : public NotificationObserver {
 public:
  // Initialize the banner with a set of preferences suitable for the given
  // options |page|. Subclasses may change that set by calling AddPref() and
  // RemovePref() afterwards.
  ManagedPrefsBannerBase(PrefService* local_state,
                         PrefService* user_prefs,
                         OptionsPage page);

  // Convenience constructor that fetches the local state PrefService from the
  // global g_browser_process.
  ManagedPrefsBannerBase(PrefService* user_prefs, OptionsPage page);

  virtual ~ManagedPrefsBannerBase();

  // Determine whether the banner should be visible.
  bool DetermineVisibility() const;

  // Add a local state preference as visibility trigger.
  void AddLocalStatePref(const char* pref);
  // Remove a local state preference from being a visibility trigger.
  void RemoveLocalStatePref(const char* pref);

  // Add a user preference as visibility trigger.
  void AddUserPref(const char* pref);
  // Remove a user preference from being a visibility trigger.
  void RemoveUserPref(const char* pref);

 protected:
  // Update banner visibility. This is called whenever a preference change is
  // observed that may lead to changed visibility of the banner. Subclasses may
  // override this in order to show/hide the banner.
  virtual void OnUpdateVisibility() { }

 private:
  // Initialization helper, called from the constructors.
  void Init(PrefService* local_state,
            PrefService* user_prefs,
            OptionsPage page);

  // |NotificationObserver| implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  scoped_ptr<PrefSetObserver> local_state_set_;
  scoped_ptr<PrefSetObserver> user_pref_set_;

  DISALLOW_COPY_AND_ASSIGN(ManagedPrefsBannerBase);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MANAGED_PREFS_BANNER_BASE_H_
