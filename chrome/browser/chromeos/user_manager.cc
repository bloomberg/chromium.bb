// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/user_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_service.h"

namespace chromeos {

// A vector pref of the users who have logged into the device.
static const wchar_t kLoggedInUsers[] = L"LoggedInUsers";

// The one true UserManager.
static UserManager* user_manager_ = NULL;

// static
UserManager* UserManager::Get() {
  if (!user_manager_) {
    g_browser_process->local_state()->RegisterListPref(kLoggedInUsers);
    user_manager_ = new UserManager();
  }
  return user_manager_;
}

std::vector<UserManager::User> UserManager::GetUsers() const {
  std::vector<User> users;

  const ListValue* prefs_users =
      g_browser_process->local_state()->GetList(kLoggedInUsers);
  if (prefs_users) {
    for (ListValue::const_iterator it = prefs_users->begin();
         it < prefs_users->end();
         ++it) {
      std::string email;
      if ((*it)->GetAsString(&email)) {
        User user;
        user.email_ = email;
        users.push_back(user);
      }
    }
  }
  return users;
}

void UserManager::UserLoggedIn(std::string email) {
  // Get a copy of the current users.
  std::vector<User> users = GetUsers();

  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListValue* prefs_users = prefs->GetMutableList(kLoggedInUsers);
  prefs_users->Clear();

  // Make sure this user is first.
  prefs_users->Append(Value::CreateStringValue(email));
  for (std::vector<User>::iterator it = users.begin();
       it < users.end();
       ++it) {
    std::string user_email = it->email_;
    // Skip the most recent user.
    if (email != user_email) {
      prefs_users->Append(Value::CreateStringValue(user_email));
    }
  }
  prefs->ScheduleSavePersistentPrefs();
}

}
