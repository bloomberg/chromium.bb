// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_CACHE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

class PrefService;

namespace chromeos {

// There is need (metrics at OOBE stage) to store settings
// (that normally would go into SignedSettings storage)
// before owner has been assigned (hence no key is available).
// This set of functions serves as a transient storage in that case.
namespace signed_settings_cache {
// Registers required pref section.
void RegisterPrefs(PrefService* local_state);

// Stores a new policy blob inside the cache stored in |local_state|.
bool Store(const em::PolicyData &policy, PrefService* local_state);

// Retrieves the policy blob from the cache stored in |local_state|.
bool Retrieve(em::PolicyData *policy, PrefService* local_state);

// Call this after owner has been assigned to persist settings
// into SignedSettings storage.
void Finalize(PrefService* local_state);
}  // namespace signed_settings_cache

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_CACHE_H_
