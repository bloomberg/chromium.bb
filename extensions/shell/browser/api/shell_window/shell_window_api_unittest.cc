// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell_window/shell_window_api.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/api_unittest.h"

using base::Value;

namespace extensions {

using ShellWindowApiTest = ApiUnitTest;

// Verifies that the show and hide functions exists and can be called without
// crashing.
TEST_F(ShellWindowApiTest, ShowHideBasics) {
  scoped_ptr<Value> result =
      RunFunctionAndReturnValue(new ShellWindowRequestHideFunction, "[]");

  // Function returns nothing.
  EXPECT_FALSE(result.get());

  result = RunFunctionAndReturnValue(new ShellWindowRequestShowFunction, "[]");

  // Function returns nothing.
  EXPECT_FALSE(result.get());

  result = RunFunctionAndReturnValue(new ShellWindowShowAppFunction, "[{}]");

  // Function returns nothing.
  EXPECT_FALSE(result.get());
}

}  // namespace extensions
