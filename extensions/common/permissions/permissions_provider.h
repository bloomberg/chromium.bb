// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_PROVIDER_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_PROVIDER_H_

#include <vector>

namespace extensions {

class APIPermissionInfo;

// The PermissionsProvider creates the APIPermissions instances. It is only
// needed at startup time.
class PermissionsProvider {
 public:
  // An alias for a given permission |name|.
  struct AliasInfo {
    const char* name;
    const char* alias;

    AliasInfo(const char* name, const char* alias)
        : name(name), alias(alias) {
    }
  };
  // Returns all the known permissions. The caller, PermissionsInfo,
  // takes ownership of the APIPermissionInfos.
  virtual std::vector<APIPermissionInfo*> GetAllPermissions() const = 0;

  // Returns all the known permission aliases.
  virtual std::vector<AliasInfo> GetAllAliases() const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_PROVIDER_H_
