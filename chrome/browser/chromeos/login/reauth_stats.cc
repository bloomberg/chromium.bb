// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/reauth_stats.h"

#include "base/metrics/histogram_macros.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

void RecordReauthReason(const std::string& user_id, ReauthReason reason) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  int old_reason;
  // We record only the first value, skipping everything else, except "none"
  // value, which is used to reset the current state.
  if (!user_manager->FindReauthReason(user_id, &old_reason) ||
      (static_cast<ReauthReason>(old_reason) == ReauthReason::NONE &&
       reason != ReauthReason::NONE)) {
    user_manager->UpdateReauthReason(user_id, static_cast<int>(reason));
  }
}

void SendReauthReason(const std::string& user_id) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  int reauth_reason;
  if (user_manager->FindReauthReason(user_id, &reauth_reason) &&
      static_cast<ReauthReason>(reauth_reason) != ReauthReason::NONE) {
    UMA_HISTOGRAM_ENUMERATION("Login.ReauthReason", reauth_reason,
                              NUM_REAUTH_FLOW_REASONS);
    user_manager->UpdateReauthReason(user_id,
                                     static_cast<int>(ReauthReason::NONE));
  }
}

}  // namespace chromeos
