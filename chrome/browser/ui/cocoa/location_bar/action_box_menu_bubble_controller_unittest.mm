// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/action_box_menu_bubble_controller.h"

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const CGFloat kAnchorPointX = 400;
const CGFloat kAnchorPointY = 300;

const int kExtension1CommandId = 0;
const int kExtension2CommandId = 1;

scoped_refptr<extensions::Extension> CreatePageLauncherExtension(
    std::string title) {
  return
      extensions::ExtensionBuilder()
      .SetManifest(extensions::DictionaryBuilder()
                   .Set("name", "Extension with page launcher")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("page_launcher", extensions::DictionaryBuilder()
                        .Set("default_title", title))
                   .Set("app", extensions::DictionaryBuilder()
                       .Set("background", extensions::DictionaryBuilder()
                           .Set("page", ""))))
      .Build();
}

class MenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  // Methods for determining the state of specific command ids.
  virtual bool IsCommandIdChecked(int command_id) const {
    return false;
  }

  virtual bool IsCommandIdEnabled(int command_id) const {
    return true;
  }

  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
    return false;
  }

  // Performs the action associated with the specified command id.
  virtual void ExecuteCommand(int command_id) {
  }
};

class ActionBoxMenuBubbleControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    // Create a test extension service.
    CommandLine command_line(CommandLine::NO_PROGRAM);
    extensions::TestExtensionSystem* test_ext_system =
        static_cast<extensions::TestExtensionSystem*>(
                extensions::ExtensionSystem::Get(profile()));
    ExtensionService* service = test_ext_system->CreateExtensionService(
        &command_line, base::FilePath(), false);
    EXPECT_TRUE(service->extensions_enabled());
    service->Init();

    // Make some fake extensions.
    extension1 = CreatePageLauncherExtension("Launch extension 1");
    service->AddExtension(extension1);

    extension2 = CreatePageLauncherExtension("Launch extension 2");
    service->AddExtension(extension2);
  }

  virtual void TearDown() OVERRIDE {
    // Close our windows.
    [controller_ close];
    CocoaProfileTest::TearDown();
  }

  void CreateController(scoped_ptr<ActionBoxMenuModel> model) {
    // The bubble controller will release itself when the window closes.
    controller_ = [[ActionBoxMenuBubbleController alloc]
         initWithModel:model.Pass()
          parentWindow:test_window()
            anchoredAt:NSMakePoint(kAnchorPointX, kAnchorPointY)
               profile:profile()];

    [controller_ showWindow:nil];
  }

 public:
  ActionBoxMenuBubbleController* controller_;
  MenuDelegate menu_delegate_;

  scoped_refptr<extensions::Extension> extension1;
  scoped_refptr<extensions::Extension> extension2;
};

TEST_F(ActionBoxMenuBubbleControllerTest, CreateMenuWithExtensions) {
  scoped_ptr<ActionBoxMenuModel> model(new ActionBoxMenuModel(
      browser(), &menu_delegate_));
  model->AddExtension(*extension1, kExtension1CommandId);
  model->AddExtension(*extension2, kExtension2CommandId);

  CreateController(model.Pass());

  // Ensure extensions are there and in the right order.
  int extension1Index = -1;
  int extension2Index = -1;
  for (id actionBoxMenuItemController in [controller_ items]) {
    NSString* label = [[actionBoxMenuItemController nameField] stringValue];
    int index = [actionBoxMenuItemController modelIndex];
    NSImage* image = [[actionBoxMenuItemController iconView] image];
    if ([label isEqualToString:@"Launch extension 1"]) {
      ASSERT_EQ(-1, extension1Index);
      ASSERT_EQ(19, [image size].width);
      ASSERT_EQ(19, [image size].height);
      extension1Index = index;
    }
    if ([label isEqualToString:@"Launch extension 2"]) {
      ASSERT_EQ(-1, extension2Index);
      ASSERT_EQ(19, [image size].width);
      ASSERT_EQ(19, [image size].height);
      extension2Index = index;
    }
  }

  ASSERT_NE(-1, extension1Index);
  ASSERT_NE(-1, extension2Index);
  ASSERT_EQ(extension1Index, extension2Index - 1);
}

}  // namespace
