// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_HELPERS_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

class PermissionSet;

namespace api {
namespace permissions {
struct Permissions;
}
}

namespace permissions_api_helpers {

// Converts the permission |set| to a permissions object.
std::unique_ptr<api::permissions::Permissions> PackPermissionSet(
    const PermissionSet& set);

// The result of unpacking the API permissions object.
struct UnpackPermissionSetResult {
  UnpackPermissionSetResult();
  ~UnpackPermissionSetResult();

  // API permissions that are in the extension's "required" permission set.
  APIPermissionSet required_apis;
  // Explicit hosts that are in the extension's "required" permission set.
  URLPatternSet required_explicit_hosts;
  // Scriptable hosts that are in the extension's "required" permission set.
  URLPatternSet required_scriptable_hosts;

  // API permissions that are in the extension's "optional" permission set.
  APIPermissionSet optional_apis;
  // API permissions that are in the extension's "optional" permission set,
  // but don't support the optional permissions API.
  APIPermissionSet unsupported_optional_apis;
  // Explicit hosts that are in the extension's "optional" permission set.
  URLPatternSet optional_explicit_hosts;

  // API permissions that were not listed in the extension's permissions.
  APIPermissionSet unlisted_apis;
  // Host permissions that were not listed in the extension's permissions.
  URLPatternSet unlisted_hosts;
};

// Parses the |permissions_input| object, and partitions permissions into the
// result. |required_permissions| and |optional_permissions| are the required
// and optional permissions specified in the extension's manifest, used for
// separating permissions. |allow_file_access| is used to determine whether the
// file:-scheme is valid for host permissions. If an error is detected (e.g.,
// an unknown API permission, invalid URL pattern, or API that doesn't support
// being optional), |error| is populated and null is returned.
std::unique_ptr<UnpackPermissionSetResult> UnpackPermissionSet(
    const api::permissions::Permissions& permissions_input,
    const PermissionSet& required_permissions,
    const PermissionSet& optional_permissions,
    bool allow_file_access,
    std::string* error);

}  // namespace permissions_api_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_HELPERS_H_
