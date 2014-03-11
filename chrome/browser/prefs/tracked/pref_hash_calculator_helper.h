// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_TRACKED_PREF_HASH_CALCULATOR_HELPER_H_
#define CHROME_BROWSER_PREFS_TRACKED_PREF_HASH_CALCULATOR_HELPER_H_

#include <string>

// Returns the legacy device ID based on |modern_device_id|. This legacy ID was
// used on prior milestones and must now be made available to the
// PrefHashCalculator to be able to continue validating hashes based on it.
// This legacy device ID is expensive to compute on some platforms and should
// thus only be computed when necessary to validate a legacy hash after which
// that hash should be migrated back to a hash based on the modern ID.
// NOTE: An empty legacy ID is a valid legacy ID.
std::string GetLegacyDeviceId(const std::string& modern_device_id);

#endif  // CHROME_BROWSER_PREFS_TRACKED_PREF_HASH_CALCULATOR_HELPER_H_
