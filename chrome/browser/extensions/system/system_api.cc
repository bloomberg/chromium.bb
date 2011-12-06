// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system/system_api.h"

#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace {

// Maps prefs::kIncognitoModeAvailability values (0 = enabled, ...)
// to strings exposed to extensions.
const char* kIncognitoModeAvailabilityStrings[] = {
  "enabled",
  "disabled",
  "forced"
};

}  // namespace

namespace extensions {

GetIncognitoModeAvailabilityFunction::~GetIncognitoModeAvailabilityFunction() {
}

bool GetIncognitoModeAvailabilityFunction::RunImpl() {
  PrefService* prefs = profile_->GetPrefs();
  int value = prefs->GetInteger(prefs::kIncognitoModeAvailability);
  EXTENSION_FUNCTION_VALIDATE(
      value >= 0 &&
      value < static_cast<int>(arraysize(kIncognitoModeAvailabilityStrings)));
  result_.reset(
      Value::CreateStringValue(kIncognitoModeAvailabilityStrings[value]));
  return true;
}

}  // namespace extensions
