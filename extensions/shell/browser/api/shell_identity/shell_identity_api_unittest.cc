// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell_identity/shell_identity_api.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/api_unittest.h"

namespace extensions {

typedef ApiUnitTest ShellIdentityApiTest;

// Verifies that the getAuthToken function exists and can be called without
// crashing.
TEST_F(ShellIdentityApiTest, GetAuthTokenBasics) {
  scoped_ptr<base::Value> result =
      RunFunctionAndReturnValue(new ShellIdentityGetAuthTokenFunction, "[]");

  // Function returns nothing.
  EXPECT_FALSE(result.get());
}

}  // namespace extensions
