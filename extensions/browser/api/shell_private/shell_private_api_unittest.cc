// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/shell_private/shell_private_api.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/api_unittest.h"

namespace extensions {

typedef ApiUnitTest ShellPrivateApiTest;

// Verifies that the printHello function exists and can be called without
// crashing.
TEST_F(ShellPrivateApiTest, PrintHello) {
  scoped_ptr<base::Value> result =
      RunFunctionAndReturnValue(new ShellPrivatePrintHelloFunction, "[]");

  // Function returns nothing.
  EXPECT_FALSE(result.get());
}

}  // namespace extensions
