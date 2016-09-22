// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_permission_context.h"

FlashPermissionContext::FlashPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            content::PermissionType::FLASH,
                            CONTENT_SETTINGS_TYPE_PLUGINS) {}

FlashPermissionContext::~FlashPermissionContext() {}

bool FlashPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}
