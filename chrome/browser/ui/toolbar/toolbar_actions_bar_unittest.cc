// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_helper.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

// A cross-platform unit test for the ToolbarActionsBar that uses the
// TestToolbarActionsBarHelper to create the platform-specific containers.
// TODO(devlin): Since this *does* use the real platform containers, in theory,
// we can move all the BrowserActionsBarBrowserTests to be unittests. See about
// doing this.
class ToolbarActionsBarUnitTest : public BrowserWithTestWindowTest {
 public:
  ToolbarActionsBarUnitTest() {}
  ~ToolbarActionsBarUnitTest() override {}

 protected:
  void SetUp() override;
  void TearDown() override;

  ToolbarActionsBar* toolbar_actions_bar() {
    return toolbar_actions_bar_helper_->GetToolbarActionsBar();
  }

 private:
  // The test helper that owns the ToolbarActionsBar and the platform-specific
  // view for it.
  scoped_ptr<TestToolbarActionsBarHelper> toolbar_actions_bar_helper_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarUnitTest);
};

void ToolbarActionsBarUnitTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  // The toolbar typically displays extension icons, so create some extension
  // test infrastructure.
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(),
      base::FilePath(),
      false);
  extensions::extension_action_test_util::CreateToolbarModelForProfile(
      profile());

  toolbar_actions_bar_helper_ = TestToolbarActionsBarHelper::Create(browser());
}

void ToolbarActionsBarUnitTest::TearDown() {
  // Since the profile gets destroyed in BrowserWithTestWindowTest::TearDown(),
  // we need to delete this now.
  toolbar_actions_bar_helper_.reset();
  BrowserWithTestWindowTest::TearDown();
}

TEST_F(ToolbarActionsBarUnitTest, BasicToolbarActionsBarTest) {
  // Add three extensions to the profile; this is the easiest way to have
  // toolbar actions.
  std::vector<scoped_refptr<const extensions::Extension>> extensions;
  for (int i = 0; i < 3; ++i) {
    extensions.push_back(
        extensions::extension_action_test_util::CreateActionExtension(
            base::StringPrintf("extension %d", i),
            extensions::extension_action_test_util::BROWSER_ACTION));
  }
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  for (const scoped_refptr<const extensions::Extension>& extension : extensions)
    extension_service->AddExtension(extension.get());

  const ToolbarActionsBar::PlatformSettings& platform_settings =
      toolbar_actions_bar()->platform_settings();

  // By default, all three actions should be visible.
  EXPECT_EQ(3u, toolbar_actions_bar()->GetIconCount());
  // Check the widths.
  int expected_width = 3 * ToolbarActionsBar::IconWidth(true) -
                       platform_settings.item_spacing +
                       platform_settings.left_padding +
                       platform_settings.right_padding;
  EXPECT_EQ(expected_width, toolbar_actions_bar()->GetPreferredSize().width());
  // Since all icons are showing, the current width should be the max width.
  int maximum_width = expected_width;
  EXPECT_EQ(maximum_width, toolbar_actions_bar()->GetMaximumWidth());
  // The minimum width should be just enough for the chevron to be displayed.
  int minimum_width = platform_settings.left_padding +
                      platform_settings.right_padding +
                      toolbar_actions_bar()->delegate_for_test()->
                          GetChevronWidth();
  EXPECT_EQ(minimum_width, toolbar_actions_bar()->GetMinimumWidth());

  // Test the connection between the ToolbarActionsBar and the model by
  // adjusting the visible count.
  extensions::ExtensionToolbarModel* toolbar_model =
      extensions::ExtensionToolbarModel::Get(profile());
  toolbar_model->SetVisibleIconCount(2u);
  EXPECT_EQ(2u, toolbar_actions_bar()->GetIconCount());

  // The current width should now be enough for two icons, and the chevron.
  expected_width = 2 * ToolbarActionsBar::IconWidth(true) -
                   platform_settings.item_spacing +
                   platform_settings.left_padding +
                   platform_settings.right_padding +
                   toolbar_actions_bar()->delegate_for_test()->
                       GetChevronWidth();
  EXPECT_EQ(expected_width, toolbar_actions_bar()->GetPreferredSize().width());
  // The maximum and minimum widths should have remained constant (since we have
  // the same number of actions).
  EXPECT_EQ(maximum_width, toolbar_actions_bar()->GetMaximumWidth());
  EXPECT_EQ(minimum_width, toolbar_actions_bar()->GetMinimumWidth());

  // Test drag-and-drop logic.
  base::string16 kExtension0 = base::ASCIIToUTF16("extension 0");
  base::string16 kExtension1 = base::ASCIIToUTF16("extension 1");
  base::string16 kExtension2 = base::ASCIIToUTF16("extension 2");

  // The order should start as 0, 1, 2.
  std::vector<ToolbarActionViewController*> toolbar_actions =
      toolbar_actions_bar()->toolbar_actions();
  EXPECT_EQ(kExtension0, toolbar_actions[0]->GetActionName());
  EXPECT_EQ(kExtension1, toolbar_actions[1]->GetActionName());
  EXPECT_EQ(kExtension2, toolbar_actions[2]->GetActionName());

  // Drag 0 to be in the second spot; 1, 0, 2, within the same container.
  toolbar_actions_bar()->OnDragDrop(0, 1, ToolbarActionsBar::DRAG_TO_SAME);
  toolbar_actions = toolbar_actions_bar()->toolbar_actions();
  EXPECT_EQ(kExtension1, toolbar_actions[0]->GetActionName());
  EXPECT_EQ(kExtension0, toolbar_actions[1]->GetActionName());
  EXPECT_EQ(kExtension2, toolbar_actions[2]->GetActionName());
  EXPECT_EQ(2u, toolbar_actions_bar()->GetIconCount());

  // Drag 0 to be in the third spot, in the overflow container.
  // Order should be 1, 2, 0, and the icon count should reduce by 1.
  toolbar_actions_bar()->OnDragDrop(1, 2, ToolbarActionsBar::DRAG_TO_OVERFLOW);
  toolbar_actions = toolbar_actions_bar()->toolbar_actions();
  EXPECT_EQ(kExtension1, toolbar_actions[0]->GetActionName());
  EXPECT_EQ(kExtension2, toolbar_actions[1]->GetActionName());
  EXPECT_EQ(kExtension0, toolbar_actions[2]->GetActionName());
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  // The model should also reflect the updated icon count.
  EXPECT_EQ(1u, toolbar_model->visible_icon_count());

  // Dragging 2 to the main container should work, even if its spot in the
  // "list" remains constant.
  // Order remains 1, 2, 0, but now we have 2 icons visible.
  toolbar_actions_bar()->OnDragDrop(1, 1, ToolbarActionsBar::DRAG_TO_MAIN);
  toolbar_actions = toolbar_actions_bar()->toolbar_actions();
  EXPECT_EQ(kExtension1, toolbar_actions[0]->GetActionName());
  EXPECT_EQ(kExtension2, toolbar_actions[1]->GetActionName());
  EXPECT_EQ(kExtension0, toolbar_actions[2]->GetActionName());
  EXPECT_EQ(2u, toolbar_actions_bar()->GetIconCount());

  // Similarly, dragging 2 to overflow, with the same "list" spot, should also
  // work. Order remains 1, 2, 0, but icon count goes back to 1.
  toolbar_actions_bar()->OnDragDrop(1, 1, ToolbarActionsBar::DRAG_TO_OVERFLOW);
  toolbar_actions = toolbar_actions_bar()->toolbar_actions();
  EXPECT_EQ(kExtension1, toolbar_actions[0]->GetActionName());
  EXPECT_EQ(kExtension2, toolbar_actions[1]->GetActionName());
  EXPECT_EQ(kExtension0, toolbar_actions[2]->GetActionName());
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());

  // Try resizing the toolbar. Start with the current width (enough for 1 icon).
  int width = toolbar_actions_bar()->GetPreferredSize().width();

  // If we try to resize by increasing, without allowing enough room for a new
  // icon, width, and icon count should stay the same.
  toolbar_actions_bar()->OnResizeComplete(width + 1);
  EXPECT_EQ(width, toolbar_actions_bar()->GetPreferredSize().width());
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());

  // If we resize by enough to include a new icon, width and icon count should
  // both increase.
  width += ToolbarActionsBar::IconWidth(true);
  toolbar_actions_bar()->OnResizeComplete(width);
  EXPECT_EQ(width, toolbar_actions_bar()->GetPreferredSize().width());
  EXPECT_EQ(2u, toolbar_actions_bar()->GetIconCount());

  // If we shrink the bar so that a full icon can't fit, it should resize to
  // hide that icon.
  toolbar_actions_bar()->OnResizeComplete(width - 1);
  width -= ToolbarActionsBar::IconWidth(true);
  EXPECT_EQ(width, toolbar_actions_bar()->GetPreferredSize().width());
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
}
