// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_SWITCHES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_SWITCHES_H_

namespace password_manager {

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

extern const char kAllowAutofillSyncCredential[];
extern const char kDisableAndroidPasswordLink[];
extern const char kDisableDropSyncCredential[];
extern const char kDisableManagerForSyncSignin[];
extern const char kDisallowAutofillSyncCredential[];
extern const char kDisallowAutofillSyncCredentialForReauth[];
extern const char kEnableAndroidPasswordLink[];
extern const char kEnableDropSyncCredential[];
extern const char kEnableManagerForSyncSignin[];
extern const char kEnableAutomaticPasswordSaving[];

}  // namespace switches

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_SWITCHES_H_
