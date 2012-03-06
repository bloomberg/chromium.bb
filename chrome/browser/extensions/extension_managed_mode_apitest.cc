// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

// Tests enabling and querying managed mode.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ManagedModeGetAndEnable) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* local_state = g_browser_process->local_state();
  ASSERT_FALSE(local_state->GetBoolean(prefs::kInManagedMode));

  ASSERT_TRUE(RunExtensionTest("managedMode")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  EXPECT_TRUE(local_state->GetBoolean(prefs::kInManagedMode));
}
