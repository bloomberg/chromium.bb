// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_COMMON_SIGNIN_SWITCHES_H_
#define COMPONENTS_SIGNIN_CORE_COMMON_SIGNIN_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// These switches should not be queried from CommandLine::HasSwitch() directly.
// Always go through the helper functions in profile_management_switches.h
// to properly take into account the state of field trials.

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kClearTokenService[];
extern const char kDisableSigninPromo[];
extern const char kDisableSigninScopedDeviceId[];
extern const char kEnableRefreshTokenAnnotationRequest[];
extern const char kEnableSigninPromo[];
extern const char kExtensionsMultiAccount[];

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Note: Account consistency is already enabled on mobile platforms, so this
// switch only exist on desktop platforms.
extern const char kEnableAccountConsistency[];
#endif

}  // namespace switches

#endif  // COMPONENTS_SIGNIN_CORE_COMMON_SIGNIN_SWITCHES_H_
