// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, CommandManifestSimple) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  const ui::Accelerator ctrl_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_CONTROL_DOWN);
  const ui::Accelerator ctrl_shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  const ui::Accelerator alt_shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess("command_simple.json");
  ASSERT_TRUE(extension);

  const extensions::CommandMap& commands = extension->named_commands();
  ASSERT_EQ(1u, commands.size());
  extensions::CommandMap::const_iterator iter = commands.begin();
  ASSERT_TRUE(commands.end() != iter);
  const extensions::Command* named_command = &(*iter).second;
  ASSERT_STREQ("feature1", named_command->command_name().c_str());
  ASSERT_STREQ("desc", UTF16ToASCII(named_command->description()).c_str());
  ASSERT_EQ(ctrl_shift_f, named_command->accelerator());

  const extensions::Command* browser_action =
      extension->browser_action_command();
  ASSERT_TRUE(NULL != browser_action);
  ASSERT_STREQ("_execute_browser_action",
               browser_action->command_name().c_str());
  ASSERT_STREQ("", UTF16ToASCII(browser_action->description()).c_str());
  ASSERT_EQ(alt_shift_f, browser_action->accelerator());

  const extensions::Command* page_action =
      extension->page_action_command();
  ASSERT_TRUE(NULL != page_action);
  ASSERT_STREQ("_execute_page_action",
      page_action->command_name().c_str());
  ASSERT_STREQ("", UTF16ToASCII(page_action->description()).c_str());
  ASSERT_EQ(ctrl_f, page_action->accelerator());
}

TEST_F(ExtensionManifestTest, CommandManifestTooMany) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  LoadAndExpectError("command_too_many.json",
                     errors::kInvalidKeyBindingTooMany);
}

TEST_F(ExtensionManifestTest, CommandManifestAllowNumbers) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess("command_allow_numbers.json");
}

TEST_F(ExtensionManifestTest, CommandManifestRejectJustShift) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  LoadAndExpectError("command_reject_just_shift.json",
      errors::kInvalidKeyBinding);
}
