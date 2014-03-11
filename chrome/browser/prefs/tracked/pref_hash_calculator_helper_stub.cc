// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/tracked/pref_hash_calculator_helper.h"

std::string GetLegacyDeviceId(const std::string& modern_device_id) {
  // On all platforms but Windows the empty string has been used as the device
  // ID since at least M34, the empty string will thus become the legacy device
  // ID on platforms that later end up providing a real device ID, it shouldn't
  // otherwise be relevant until then.
  return std::string();
}
