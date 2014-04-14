// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_permissions_provider.h"

namespace extensions {

TestPermissionsProvider::TestPermissionsProvider() {}

TestPermissionsProvider::~TestPermissionsProvider() {}

std::vector<APIPermissionInfo*> TestPermissionsProvider::GetAllPermissions()
    const {
  // TODO(tfarina): This needs to have a "real" implementation, otherwise
  // some tests under extensions_unittests will fail. http://crbug.com/348066
  std::vector<APIPermissionInfo*> permissions;
  return permissions;
}

std::vector<PermissionsProvider::AliasInfo>
TestPermissionsProvider::GetAllAliases() const {
  std::vector<PermissionsProvider::AliasInfo> aliases;
  return aliases;
}

}  // namespace extensions
