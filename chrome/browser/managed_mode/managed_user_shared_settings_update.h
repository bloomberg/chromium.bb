// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SHARED_SETTINGS_UPDATE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SHARED_SETTINGS_UPDATE_H_

#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class Value;
}

class ManagedUserSharedSettingsService;

// Lets clients of ManagedUserSharedSettingsService change settings and wait for
// the Sync server to acknowledge the change. The callback passed in the
// constructor will be called with a true success value after the Sync server
// has flipped the acknowledgement flag for the setting. If another client
// changes the value in the mean time, the callback will be run with a false
// success value. If the object is destroyed before that, the callback will not
// be run. Note that any changes made to the setting will not be undone when
// destroying the object, even if the update was not successful or was canceled.
class ManagedUserSharedSettingsUpdate {
 public:
  ManagedUserSharedSettingsUpdate(
      ManagedUserSharedSettingsService* service,
      const std::string& mu_id,
      const std::string& key,
      scoped_ptr<base::Value> value,
      const base::Callback<void(bool)>& success_callback);
  ~ManagedUserSharedSettingsUpdate();

 private:
  typedef base::CallbackList<void(const std::string&, const std::string&)>
      CallbackList;

  void OnSettingChanged(const std::string& mu_id,
                        const std::string& key);

  ManagedUserSharedSettingsService* service_;
  std::string mu_id_;
  std::string key_;
  scoped_ptr<base::Value> value_;
  base::Callback<void(bool)> callback_;
  scoped_ptr<CallbackList::Subscription> subscription_;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SHARED_SETTINGS_UPDATE_H_
