// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MULTI_PROFILE_FIRST_RUN_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MULTI_PROFILE_FIRST_RUN_NOTIFICATION_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

namespace chromeos {

class MultiProfileFirstRunNotification {
 public:
  MultiProfileFirstRunNotification();
  ~MultiProfileFirstRunNotification();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Invoked when the user profile is prepared. Show the notification for
  // the primary user when multiprofile is enabled and user has not dismissed
  // it yet.
  void UserProfilePrepared(Profile* user_profile);

 private:
  // Invoked when user dismisses the notification.
  void OnDismissed(Profile* user_profile);

  base::WeakPtrFactory<MultiProfileFirstRunNotification> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileFirstRunNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MULTI_PROFILE_FIRST_RUN_NOTIFICATION_H_
