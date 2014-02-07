// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace prefs {

// A list of numbers. Each number corresponds to one of the domains monitored
// for save-password-prompt breakages. That number is a random index into
// the array of groups containing the monitored domain. That group should be
// used for reporting that domain.
const char kPasswordManagerGroupsForDomains[] =
    "profile.password_manager_groups_for_domains";

}  // namespace prefs
