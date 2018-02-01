// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_PERMISSIONS_PARSER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_PERMISSIONS_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"

namespace extensions {

class APIPermissionSet;
class Extension;
class URLPatternSet;

// The class for parsing the kPermissions and kOptionalPermissions keys in the
// manifest. Because permissions are slightly different than other keys (they
// are used in many different handlers and need to be the first and last key
// touched), this is not an actual ManifestHandler (hence the difference in
// name).
class PermissionsParser {
 public:
  PermissionsParser();
  ~PermissionsParser();

  // Parse the manifest-specified permissions.
  bool Parse(Extension* extension, base::string16* error);

  // Finalize the permissions, setting the related manifest data on the
  // extension.
  void Finalize(Extension* extension);

  // A utility method to parse host permissions from a list of match patterns.
  // Also returns match patterns which couldn't be parsed as |malformed_hosts|
  // and hosts with invalid schemes as |invalid_scheme_hosts|. These must be
  // non-null.
  // TODO(crbug.com/777714): Provide an option to tweak behavior for chrome
  // scheme hosts.
  static void ParseHostPermissions(
      Extension* extension,
      const std::vector<std::string>& host_data,
      const APIPermissionSet& api_permissions,
      URLPatternSet* host_permissions,
      std::vector<std::string>* malformed_hosts,
      std::vector<std::string>* invalid_scheme_hosts);

  // Modify the manifest permissions. These methods should only be used
  // during initialization and will DCHECK() for safety.
  static void AddAPIPermission(Extension* extension,
                               APIPermission::ID permission);
  static void AddAPIPermission(Extension* extension, APIPermission* permission);
  static bool HasAPIPermission(const Extension* extension,
                               APIPermission::ID permission);
  static const APIPermissionSet& GetRequiredAPIPermissions(
      const Extension* extension);
  static void SetScriptableHosts(Extension* extension,
                                 const URLPatternSet& scriptable_hosts);
  // Sets host permissions for the Declarative Net Request API.
  static void SetHostPermissionsForDNR(Extension* extension,
                                       URLPatternSet dnr_hosts);

  // Return the extension's manifest-specified permissions. In no cases should
  // these permissions be used to determine if an action is allowed. Instead,
  // use PermissionsData.
  static const PermissionSet& GetRequiredPermissions(
      const Extension* extension);
  static const PermissionSet& GetOptionalPermissions(
      const Extension* extension);

 private:
  struct InitialPermissions;

  // The initial permissions for the extension, which can still be modified.
  std::unique_ptr<InitialPermissions> initial_required_permissions_;
  std::unique_ptr<InitialPermissions> initial_optional_permissions_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_PERMISSIONS_PARSER_H_
