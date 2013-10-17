// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

typedef ExtensionApiTest CommandServiceTest;

IN_PROC_BROWSER_TEST_F(CommandServiceTest, RemoveShortcutSurvivesUpdate) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v1"),
      scoped_temp_dir.path().AppendASCII("v1.crx"),
      pem_path,
      base::FilePath());
  base::FilePath path_v2 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding").AppendASCII("update")
                    .AppendASCII("v2"),
      scoped_temp_dir.path().AppendASCII("v2.crx"),
      pem_path,
      base::FilePath());

  ExtensionService* service = ExtensionSystem::Get(browser()->profile())->
      extension_service();
  CommandService* command_service = CommandService::Get(browser()->profile());

  const char kId[] = "pgoakhfeplldmjheffidklpoklkppipp";

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(service->GetExtensionById(kId, false) != NULL);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Remove the keybinding.
  command_service->RemoveKeybindingPrefs(
      kId, manifest_values::kBrowserActionCommandEvent);

  // Verify it got removed.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Update to version 2.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(service->GetExtensionById(kId, false) != NULL);

  // Verify it is still set to nothing.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());
}

}  // namespace extensions
