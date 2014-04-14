// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_PERMISSIONS_PROVIDER_H_
#define EXTENSIONS_TEST_TEST_PERMISSIONS_PROVIDER_H_

#include "base/macros.h"
#include "extensions/common/permissions/permissions_provider.h"

namespace extensions {

class TestPermissionsProvider : public PermissionsProvider {
 public:
  TestPermissionsProvider();
  virtual ~TestPermissionsProvider();

 private:
  // PermissionsProvider:
  virtual std::vector<APIPermissionInfo*> GetAllPermissions() const OVERRIDE;
  virtual std::vector<AliasInfo> GetAllAliases() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TestPermissionsProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_PERMISSIONS_PROVIDER_H_
