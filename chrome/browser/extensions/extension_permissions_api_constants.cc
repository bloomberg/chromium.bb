// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_permissions_api_constants.h"

namespace extension_permissions_module_constants {

const char kApisKey[] = "permissions";
const char kOriginsKey[] = "origins";

const char kCantRemoveRequiredPermissionsError[] =
    "You cannot remove required permissions.";
const char kNotInOptionalPermissionsError[] =
    "Optional permissions must be listed in extension manifest.";
const char kNotWhitelistedError[] =
    "The optional permissions API does not support '*'.";
const char kUnknownPermissionError[] =
    "'*' is not a recognized permission.";
const char kInvalidOrigin[] =
    "Invalid value for origin pattern *: *";

const char kOnAdded[] = "experimental.permissions.onAdded";
const char kOnRemoved[] = "experimental.permissions.onRemoved";

};  // namespace extension_permissions_module_constants
