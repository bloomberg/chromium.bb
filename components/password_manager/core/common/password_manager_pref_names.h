// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_

#include "base/basictypes.h"

namespace password_manager {

namespace prefs {

// Alphabetical list of preference names specific to the PasswordManager
// component. Keep alphabetized, and document each in the .cc file.
#if defined(OS_WIN)
extern const char kOsPasswordBlank[];
extern const char kOsPasswordLastChanged[];
#endif
extern const char kPasswordManagerAllowShowPasswords[];
extern const char kPasswordManagerSavingEnabled[];
extern const char kPasswordManagerGroupsForDomains[];

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
extern const char kLocalProfileId[];
#endif

}  // namespace prefs

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
