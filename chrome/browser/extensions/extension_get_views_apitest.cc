// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

#if defined(USE_AURA)
// crbug.com/105177.
#define MAYBE_GetViews DISABLED_GetViews
#else
#define MAYBE_GetViews GetViews
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_GetViews) {
  // TODO(finnur): Remove once infobars are no longer experimental (bug 39511).
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("get_views")) << message_;
}
