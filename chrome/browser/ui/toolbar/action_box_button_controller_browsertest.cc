// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/toolbar/action_box_button_controller.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

// ActionBoxButtonControllerTest ----------------------------------------------

class ActionBoxButtonControllerTest
    : public ExtensionBrowserTest,
      public ActionBoxButtonController::Delegate {
 public:
  virtual void SetUpOnMainThread() OVERRIDE;

  void OpenMenu();

  // Installs the page launcher extension.
  const Extension* Install();

  // These methods operate on the model the controller gave us on the last
  // call to OpenMenu().
  int GetIndexOfExtension(const Extension* extension);
  int GetCommandId(const Extension* extension);
  void ExecuteCommand(int command_id);

 private:
  virtual void ShowMenu(scoped_ptr<ActionBoxMenuModel> menu_model) OVERRIDE;

  scoped_ptr<ActionBoxButtonController> controller_;
  scoped_ptr<ActionBoxMenuModel> model_;
};

void ActionBoxButtonControllerTest::SetUpOnMainThread() {
  controller_.reset(new ActionBoxButtonController(browser(), this));
}

void ActionBoxButtonControllerTest::OpenMenu() {
  controller_->OnButtonClicked();
  ASSERT_TRUE(model_.get());
}

const Extension* ActionBoxButtonControllerTest::Install() {
  return ExtensionBrowserTest::InstallExtension(
      test_data_dir_.AppendASCII("action_box_button_controller"),
      1);
}

void ActionBoxButtonControllerTest::ShowMenu(
    scoped_ptr<ActionBoxMenuModel> menu_model) {
  model_ = menu_model.Pass();
}

int ActionBoxButtonControllerTest::GetIndexOfExtension(
    const Extension* extension) {
  for (int i = 0; i < model_->GetItemCount(); i++) {
    if (model_->IsItemExtension(i) &&
        model_->GetExtensionAt(i)->id() == extension->id())
      return i;
  }
  return -1;
}

int ActionBoxButtonControllerTest::GetCommandId(const Extension* extension) {
  return model_->GetCommandIdAt(GetIndexOfExtension(extension));
}

void ActionBoxButtonControllerTest::ExecuteCommand(int command_id) {
  model_->ExecuteCommand(command_id);
}

// Actual tests ---------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(ActionBoxButtonControllerTest, ExtensionVisible) {
  const Extension* extension = Install();
  ASSERT_TRUE(extension);

  // Make sure the extension shows up in the menu.
  OpenMenu();
  EXPECT_GE(GetIndexOfExtension(extension), 0);

  // Disabled extensions should no longer show up.
  DisableExtension(extension->id());
  OpenMenu();
  EXPECT_EQ(-1, GetIndexOfExtension(extension));

  // Re-enabling the extension should bring it back.
  EnableExtension(extension->id());
  OpenMenu();
  EXPECT_GE(GetIndexOfExtension(extension), 0);
}

IN_PROC_BROWSER_TEST_F(ActionBoxButtonControllerTest,
                       ExecuteExtension) {
  const Extension* extension = Install();
  ASSERT_TRUE(extension);

  OpenMenu();
  int command_id = GetCommandId(extension);

  // Executing the command should trigger pageLauncher.onClicked.
  ExtensionTestMessageListener listener("onClicked", false);
  ExecuteCommand(command_id);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ActionBoxButtonControllerTest,
                       ExecuteDisabledExtension) {
  const Extension* extension = Install();
  ASSERT_TRUE(extension);

  // Open the menu while extension is enabled.
  OpenMenu();
  int command_id = GetCommandId(extension);

  // Make sure we don't crash if we try to execute a (now) disabled extension.
  DisableExtension(extension->id());
  ExecuteCommand(command_id);
}

IN_PROC_BROWSER_TEST_F(ActionBoxButtonControllerTest,
                       ExecuteUninstalledExtension) {
  const Extension* extension = Install();
  ASSERT_TRUE(extension);

  // Open the menu while extension is installed.
  OpenMenu();
  int command_id = GetCommandId(extension);

  // Make sure we don't crash even if the extension isn't installed any more.
  UninstallExtension(extension->id());
  ExecuteCommand(command_id);
}
