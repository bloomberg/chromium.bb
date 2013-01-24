// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

namespace extensions {

class CommandsManifestTest : public ExtensionManifestTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ManifestHandler::Register(extension_manifest_keys::kCommands,
                              new CommandsHandler);
  }
};

TEST_F(CommandsManifestTest, CommandManifestSimple) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

#if defined(OS_MACOSX)
  int ctrl = ui::EF_COMMAND_DOWN;
#else
  int ctrl = ui::EF_CONTROL_DOWN;
#endif

  const ui::Accelerator ctrl_f = ui::Accelerator(ui::VKEY_F, ctrl);
  const ui::Accelerator ctrl_shift_f =
      ui::Accelerator(ui::VKEY_F, ctrl | ui::EF_SHIFT_DOWN);
  const ui::Accelerator alt_shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_simple.json");
  ASSERT_TRUE(extension);

  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension);
  ASSERT_TRUE(commands);
  ASSERT_EQ(1u, commands->size());
  CommandMap::const_iterator iter = commands->begin();
  ASSERT_TRUE(commands->end() != iter);
  const Command* named_command = &(*iter).second;
  ASSERT_STREQ("feature1", named_command->command_name().c_str());
  ASSERT_STREQ("desc", UTF16ToASCII(named_command->description()).c_str());
  ASSERT_EQ(ctrl_shift_f, named_command->accelerator());

  const Command* browser_action =
      CommandsInfo::GetBrowserActionCommand(extension);
  ASSERT_TRUE(NULL != browser_action);
  ASSERT_STREQ("_execute_browser_action",
               browser_action->command_name().c_str());
  ASSERT_STREQ("", UTF16ToASCII(browser_action->description()).c_str());
  ASSERT_EQ(alt_shift_f, browser_action->accelerator());

  const Command* page_action = CommandsInfo::GetPageActionCommand(extension);
  ASSERT_TRUE(NULL != page_action);
  ASSERT_STREQ("_execute_page_action",
      page_action->command_name().c_str());
  ASSERT_STREQ("", UTF16ToASCII(page_action->description()).c_str());
  ASSERT_EQ(ctrl_f, page_action->accelerator());
}

TEST_F(CommandsManifestTest, CommandManifestTooMany) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  LoadAndExpectError("command_too_many.json",
                     errors::kInvalidKeyBindingTooMany);
}

TEST_F(CommandsManifestTest, CommandManifestAllowNumbers) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_allow_numbers.json");
}

TEST_F(CommandsManifestTest, CommandManifestRejectJustShift) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  LoadAndExpectError("command_reject_just_shift.json",
      errors::kInvalidKeyBinding);
}

}  // namespace extensions
