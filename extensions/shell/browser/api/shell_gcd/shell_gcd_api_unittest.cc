// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell_gcd/shell_gcd_api.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/api_unittest.h"

namespace extensions {

using ShellGcdApiTest = ApiUnitTest;

TEST_F(ShellGcdApiTest, GetBootstrapStatus) {
  // Function succeeds and returns a result (for its callback).
  scoped_ptr<base::Value> result =
      RunFunctionAndReturnValue(new ShellGcdGetSetupStatusFunction, "[{}]");
  ASSERT_TRUE(result.get());
  std::string value;
  result->GetAsString(&value);
  EXPECT_EQ("completed", value);
}

}  // namespace extensions
