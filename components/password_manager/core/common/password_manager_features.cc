// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

namespace features {

// Disables the save-password prompt. Passwords are then saved automatically,
// without asking the user.
const base::Feature kEnableAutomaticPasswordSaving = {
    "enable-automatic-password-saving", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable supporting of updating password in the password manager on a password
// change form submit.
const base::Feature kEnablePasswordChangeSupport = {
    "enable-password-change-support", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable a context menu item in the password field that allows the user
// to manually enforce saving of their password.
const base::Feature kEnablePasswordForceSaving = {
    "enable-password-force-saving", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the user to trigger password generation manually.
extern const base::Feature kEnableManualPasswordGeneration = {
    "enable-manual-password-generation", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

}  // namespace password_manager
