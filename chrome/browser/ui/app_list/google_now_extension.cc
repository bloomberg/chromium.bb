// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/google_now_extension.h"

#include "components/variations/variations_associated_data.h"

namespace {
const char kGoogleNowExtensionFieldTrialName[] = "GoogleNowExtension";
};  // namespace

bool GetGoogleNowExtensionId(std::string* extension_id) {
  *extension_id = variations::GetVariationParamValue(
      kGoogleNowExtensionFieldTrialName, "id");
  return !extension_id->empty();
}
