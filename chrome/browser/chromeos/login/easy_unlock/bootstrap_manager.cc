// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/bootstrap_manager.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"

namespace chromeos {

namespace {

// A pref list of users who have not finished Easy bootstrapping.
const char kPendingEasyBootstrapUsers[] = "PendingEasyBootstrapUsers";

}  // namespace

// static
void BootstrapManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kPendingEasyBootstrapUsers);
}

BootstrapManager::BootstrapManager(Delegate* delegate)
    : delegate_(delegate) {
}

BootstrapManager::~BootstrapManager() {
}

void BootstrapManager::AddPendingBootstrap(const std::string& user_id) {
  DCHECK(!user_id.empty());
  PrefService* local_state = g_browser_process->local_state();

  ListPrefUpdate update(local_state, kPendingEasyBootstrapUsers);
  update->AppendString(user_id);
}

void BootstrapManager::FinishPendingBootstrap(const std::string& user_id) {
  PrefService* local_state = g_browser_process->local_state();

  ListPrefUpdate update(local_state, kPendingEasyBootstrapUsers);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    std::string current_user;
    if (update->GetString(i, &current_user) && user_id == current_user) {
      update->Remove(i, NULL);
      break;
    }
  }
}

void BootstrapManager::RemoveAllPendingBootstrap() {
  PrefService* local_state = g_browser_process->local_state();

  const base::ListValue* users =
      local_state->GetList(kPendingEasyBootstrapUsers);
  for (size_t i = 0; i < users->GetSize(); ++i) {
    std::string current_user;
    if (users->GetString(i, &current_user))
      delegate_->RemovePendingBootstrapUser(current_user);
  }

  local_state->ClearPref(kPendingEasyBootstrapUsers);
  local_state->CommitPendingWrite();
}

bool BootstrapManager::HasPendingBootstrap(const std::string& user_id) const {
  PrefService* local_state = g_browser_process->local_state();

  const base::ListValue* users =
      local_state->GetList(kPendingEasyBootstrapUsers);
  for (size_t i = 0; i < users->GetSize(); ++i) {
    std::string current_user;
    if (users->GetString(i, &current_user) && user_id == current_user)
      return true;
  }
  return false;
}

}  // namespace chromeos
