// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Clear) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("clear/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ClearOneAtATime) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
            switches::kEnableExperimentalExtensionApis);
  BrowsingDataRemover::set_removing(true);
  ASSERT_TRUE(RunExtensionTest("clear/one_at_a_time")) << message_;
  BrowsingDataRemover::set_removing(false);
}
