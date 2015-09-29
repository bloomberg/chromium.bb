// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_PROVIDER_H_

#include "base/basictypes.h"

namespace sessions {
class LiveTab;
class TabRestoreServiceDelegate;
}

namespace ios {

class ChromeBrowserState;

// A class that allows retrieving a sessions::TabRestoreServiceDelegate.
class TabRestoreServiceDelegateProvider {
 public:
  virtual ~TabRestoreServiceDelegateProvider() {}

  // Creates a new delegate.
  virtual sessions::TabRestoreServiceDelegate* Create(
      ios::ChromeBrowserState* browser_state) = 0;

  // Retrieves a delegate with the given ID, if one exists.
  virtual sessions::TabRestoreServiceDelegate* FindDelegateWithID(
      int32 desired_id) = 0;

  // Retrieves a delegate for the given tab, if one exists.
  virtual sessions::TabRestoreServiceDelegate* FindDelegateForTab(
      const sessions::LiveTab* tab) = 0;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_PROVIDER_H_
