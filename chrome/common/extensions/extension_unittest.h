// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_UNITTEST_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_UNITTEST_H_

#include "base/compiler_specific.h"
#include "chrome/common/extensions/permissions/scoped_testing_permissions_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// A base class for extension unit tests that sets up permissions and
// universally useful manifest handlers.
class ExtensionTest : public testing::Test {
 public:
  ExtensionTest();

 protected:
  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

  ScopedTestingPermissionsInfo permissions_info_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_UNITTEST_H_
