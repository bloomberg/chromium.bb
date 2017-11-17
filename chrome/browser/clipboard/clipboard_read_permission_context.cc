// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/clipboard/clipboard_read_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/WebKit/common/feature_policy/feature_policy_feature.h"

ClipboardReadPermissionContext::ClipboardReadPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            CONTENT_SETTINGS_TYPE_CLIPBOARD_READ,
                            blink::FeaturePolicyFeature::kNotFound) {}

ClipboardReadPermissionContext::~ClipboardReadPermissionContext() {}

bool ClipboardReadPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
