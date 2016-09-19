// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugins_permission_context.h"

PluginsPermissionContext::PluginsPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            content::PermissionType::PLUGINS,
                            CONTENT_SETTINGS_TYPE_PLUGINS) {}

PluginsPermissionContext::~PluginsPermissionContext() {}

bool PluginsPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}
