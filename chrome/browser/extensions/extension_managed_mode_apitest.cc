// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// Tests enabling and querying managed mode.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ManagedModeGetAndEnable) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_FALSE(ManagedMode::IsInManagedMode());

  ASSERT_TRUE(RunComponentExtensionTest("managedMode")) << message_;

  EXPECT_TRUE(ManagedMode::IsInManagedMode());
}
