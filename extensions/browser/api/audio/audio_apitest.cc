// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_apitest.h"

namespace extensions {

using AudioApiTest = ShellApiTest;

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AudioApiTest, Audio) {
  EXPECT_TRUE(RunAppTest("api_test/audio")) << message_;
}
#endif

}  // namespace extensions
