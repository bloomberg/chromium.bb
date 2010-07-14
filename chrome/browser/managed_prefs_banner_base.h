// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_PREFS_BANNER_BASE_H_
#define CHROME_BROWSER_MANAGED_PREFS_BANNER_BASE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "chrome/common/notification_observer.h"

class PrefService;

// Common base functionality for the managed prefs warning banner displayed in
// the preference dialogs when there are options that are controlled by
// configuration policy and thus cannot be changed by the user.
class ManagedPrefsBannerBase : public NotificationObserver {
 public:
  ManagedPrefsBannerBase(PrefService* prefs,
                         const wchar_t** relevant_prefs,
                         size_t count);
  virtual ~ManagedPrefsBannerBase();

  // Determine whether the banner should be visible.
  bool DetermineVisibility() const;

  // |NotificationObserver| implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // Update banner visibility. This is called whenever a preference change is
  // observed that may lead to changed visibility of the banner. Subclasses may
  // override this in order to show/hide the banner.
  virtual void OnUpdateVisibility() { }

 private:
  PrefService* prefs_;
  typedef std::set<std::wstring> PrefSet;
  PrefSet relevant_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ManagedPrefsBannerBase);
};
#endif  // CHROME_BROWSER_MANAGED_PREFS_BANNER_BASE_H_
