// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_commands.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionCommandsTest : public testing::Test {
};

TEST(ExtensionCommandsTest, ExtensionCommandParsing) {
  const ui::Accelerator none = ui::Accelerator();
  const ui::Accelerator shift_f = ui::Accelerator(ui::VKEY_F,
                                                  ui::EF_SHIFT_DOWN);
  const ui::Accelerator ctrl_f = ui::Accelerator(ui::VKEY_F,
                                                ui::EF_CONTROL_DOWN);
  const ui::Accelerator alt_f = ui::Accelerator(ui::VKEY_F, ui::EF_ALT_DOWN);
  const ui::Accelerator ctrl_shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  const ui::Accelerator alt_shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  const struct {
    bool expected_result;
    ui::Accelerator accelerator;
    const char* command_name;
    const char* key;
    const char* description;
  } kTests[] = {
    // Negative test (one or more missing required fields). We don't need to
    // test |command_name| being blank as it is used as a key in the manifest,
    // so it can't be blank (and we CHECK() when it is).
    { false, none, "command", "",       "" },
    { false, none, "command", "Ctrl+f", "" },
    { false, none, "command", "",       "description" },
    // Ctrl+Alt is not permitted, see MSDN link in comments in Parse function.
    { false, none, "command", "Ctrl+Alt+F", "description" },
    // Unsupported shortcuts/too many, or missing modifier.
    { false, none, "command", "A",                "description" },
    { false, none, "command", "F10",              "description" },
    { false, none, "command", "Ctrl+1",           "description" },
    { false, none, "command", "Ctrl+F+G",         "description" },
    { false, none, "command", "Ctrl+Alt+Shift+G", "description" },
    // Basic tests.
    { true, ctrl_f,      "command", "Ctrl+F",       "description" },
    { true, shift_f,     "command", "Shift+F",      "description" },
    { true, alt_f,       "command", "Alt+F",        "description" },
    { true, ctrl_shift_f, "command", "Ctrl+Shift+F", "description" },
    { true, alt_shift_f,  "command", "Alt+Shift+F",  "description" },
    // Order tests.
    { true, ctrl_f,      "command", "F+Ctrl",       "description" },
    { true, shift_f,     "command", "F+Shift",      "description" },
    { true, alt_f,       "command", "F+Alt",        "description" },
    { true, ctrl_shift_f, "command", "F+Ctrl+Shift", "description" },
    { true, ctrl_shift_f, "command", "F+Shift+Ctrl", "description" },
    { true, alt_shift_f,  "command", "F+Alt+Shift",  "description" },
    { true, alt_shift_f,  "command", "F+Shift+Alt",  "description" },
    // Case insensitivity is not OK.
    { false, ctrl_f, "command", "Ctrl+f", "description" },
    { false, ctrl_f, "command", "cTrL+F", "description" },
    // Skipping description is OK for browser- and pageActions.
    { true, ctrl_f, "_execute_browser_action", "Ctrl+F", "" },
    { true, ctrl_f, "_execute_page_action",    "Ctrl+F", "" },
  };

  // TODO(finnur): test Command/Options on Mac when implemented.

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    // First parse the command as a simple string.
    scoped_ptr<DictionaryValue> input(new DictionaryValue);
    input->SetString("suggested_key", kTests[i].key);
    input->SetString("description", kTests[i].description);

    SCOPED_TRACE(std::string("Command name: |") + kTests[i].command_name +
                 "| key: |" + kTests[i].key +
                 "| description: |" + kTests[i].description +
                 "| index: " + base::IntToString(i));

    extensions::Command command;
    string16 error;
    bool result =
        command.Parse(input.get(), kTests[i].command_name, i, &error);

    EXPECT_EQ(kTests[i].expected_result, result);
    if (result) {
      EXPECT_STREQ(kTests[i].description, command.description().c_str());
      EXPECT_STREQ(kTests[i].command_name, command.command_name().c_str());
      EXPECT_EQ(kTests[i].accelerator, command.accelerator());
    }

    // Now parse the command as a dictionary of multiple values.
    input.reset(new DictionaryValue);
    DictionaryValue* key_dict = new DictionaryValue();
    key_dict->SetString("default", kTests[i].key);
    key_dict->SetString("windows", kTests[i].key);
    key_dict->SetString("mac", kTests[i].key);
    input->Set("suggested_key", key_dict);
    input->SetString("description", kTests[i].description);

    result = command.Parse(input.get(), kTests[i].command_name, i, &error);

    EXPECT_EQ(kTests[i].expected_result, result);
    if (result) {
      EXPECT_STREQ(kTests[i].description, command.description().c_str());
      EXPECT_STREQ(kTests[i].command_name, command.command_name().c_str());
      EXPECT_EQ(kTests[i].accelerator, command.accelerator());
    }
  }
}

TEST(ExtensionCommandsTest, ExtensionCommandParsingFallback) {
  std::string description = "desc";
  std::string command_name = "foo";

  // Test that platform specific keys are honored on each platform, despite
  // fallback being given.
  scoped_ptr<DictionaryValue> input(new DictionaryValue);
  DictionaryValue* key_dict = new DictionaryValue();
  key_dict->SetString("default",  "Ctrl+Shift+D");
  key_dict->SetString("windows",  "Ctrl+Shift+W");
  key_dict->SetString("mac",      "Ctrl+Shift+M");
  key_dict->SetString("linux",    "Ctrl+Shift+L");
  key_dict->SetString("chromeos", "Ctrl+Shift+C");
  input->Set("suggested_key", key_dict);
  input->SetString("description", description);

  extensions::Command command;
  string16 error;
  EXPECT_TRUE(command.Parse(input.get(), command_name, 0, &error));
  EXPECT_STREQ(description.c_str(), command.description().c_str());
  EXPECT_STREQ(command_name.c_str(), command.command_name().c_str());

#if defined(OS_WIN)
  ui::Accelerator accelerator(ui::VKEY_W, true, true, false);
#elif defined(OS_MACOSX)
  ui::Accelerator accelerator(ui::VKEY_M, true, true, false);
#elif defined(OS_CHROMEOS)
  ui::Accelerator accelerator(ui::VKEY_C, true, true, false);
#elif defined(OS_LINUX)
  ui::Accelerator accelerator(ui::VKEY_L, true, true, false);
#else
  ui::Accelerator accelerator(ui::VKEY_D, true, true, false);
#endif
  EXPECT_EQ(accelerator, command.accelerator());

  // Misspell a platform.
  key_dict->SetString("windosw", "Ctrl+M");
  EXPECT_FALSE(command.Parse(input.get(), command_name, 0, &error));
  EXPECT_TRUE(key_dict->Remove("windosw", NULL));

  // Now remove platform specific keys (leaving just "default") and make sure
  // every platform falls back to the default.
  EXPECT_TRUE(key_dict->Remove("windows", NULL));
  EXPECT_TRUE(key_dict->Remove("mac", NULL));
  EXPECT_TRUE(key_dict->Remove("linux", NULL));
  EXPECT_TRUE(key_dict->Remove("chromeos", NULL));
  EXPECT_TRUE(command.Parse(input.get(), command_name, 0, &error));
  EXPECT_EQ(ui::VKEY_D, command.accelerator().key_code());

  // Now remove "default", leaving no option but failure. Or, in the words of
  // the immortal Adam Savage: "Failure is always an option".
  EXPECT_TRUE(key_dict->Remove("default", NULL));
  EXPECT_FALSE(command.Parse(input.get(), command_name, 0, &error));

  // Now add only a valid platform that we are not running on to make sure devs
  // are notified of errors on other platforms.
#if defined(OS_WIN)
  key_dict->SetString("mac", "Ctrl+Shift+M");
#else
  key_dict->SetString("windows", "Ctrl+Shift+W");
#endif
  EXPECT_FALSE(command.Parse(input.get(), command_name, 0, &error));

  // Make sure Mac specific keys are not processed on other platforms.
#if !defined(OS_MACOSX)
  key_dict->SetString("windows", "Command+Shift+M");
  EXPECT_FALSE(command.Parse(input.get(), command_name, 0, &error));
  key_dict->SetString("windows", "Options+Shift+M");
  EXPECT_FALSE(command.Parse(input.get(), command_name, 0, &error));
#endif
}
