// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_helper.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"

// A cross-platform unit test for the ToolbarActionsBar that uses the
// TestToolbarActionsBarHelper to create the platform-specific containers.
// TODO(devlin): Since this *does* use the real platform containers, in theory,
// we can move all the BrowserActionsBarBrowserTests to be unittests. See about
// doing this.
class ToolbarActionsBarUnitTest : public BrowserWithTestWindowTest {
 public:
  ToolbarActionsBarUnitTest() : toolbar_model_(nullptr) {}
  ~ToolbarActionsBarUnitTest() override {}

 protected:
  void SetUp() override;
  void TearDown() override;

  // Activates the tab at the given |index| in the tab strip model.
  void ActivateTab(int index);

  // Set whether or not the given |action| wants to run on the |web_contents|.
  void SetActionWantsToRunOnTab(ExtensionAction* action,
                                content::WebContents* web_contents,
                                bool wants_to_run);

  // Creates an extension with the given |name| and |action_type|, adds it to
  // the associated extension service, and returns the created extension. (It's
  // safe to ignore the returned value.)
  scoped_refptr<const extensions::Extension> CreateAndAddExtension(
      const std::string& name,
      extensions::extension_action_test_util::ActionType action_type);

  // Verifies that the toolbar is in order specified by |expected_names|, has
  // the total action count of |total_size|, and has the same |visible_count|.
  // This verifies that both the ToolbarActionsBar and the associated
  // (platform-specific) view is correct.
  // We use expected names (instead of ids) because they're much more readable
  // in a debug message. These aren't enforced to be unique, so don't make
  // duplicates.
  // If any of these is wrong, returns testing::AssertionFailure() with a
  // message.
  testing::AssertionResult VerifyToolbarOrder(
      const char* expected_names[],
      size_t total_size,
      size_t visible_count) WARN_UNUSED_RESULT;

  ToolbarActionsBar* toolbar_actions_bar() {
    return toolbar_actions_bar_helper_->GetToolbarActionsBar();
  }
  extensions::ExtensionToolbarModel* toolbar_model() {
    return toolbar_model_;
  }
  BrowserActionTestUtil* browser_action_test_util() {
    return browser_action_test_util_.get();
  }

 private:
  // The test helper that owns the ToolbarActionsBar and the platform-specific
  // view for it.
  scoped_ptr<TestToolbarActionsBarHelper> toolbar_actions_bar_helper_;

  // The associated ExtensionToolbarModel (owned by the keyed service setup).
  extensions::ExtensionToolbarModel* toolbar_model_;

  // A BrowserActionTestUtil object constructed with the associated
  // ToolbarActionsBar.
  scoped_ptr<BrowserActionTestUtil> browser_action_test_util_;

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
  toolbar_model_ =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile());

  toolbar_actions_bar_helper_ = TestToolbarActionsBarHelper::Create(browser());

  BrowserActionTestUtil::DisableAnimations();
  browser_action_test_util_.reset(
      new BrowserActionTestUtil(browser(),
                                toolbar_actions_bar()->delegate_for_test()));
}

void ToolbarActionsBarUnitTest::TearDown() {
  // Since the profile gets destroyed in BrowserWithTestWindowTest::TearDown(),
  // we need to delete this now.
  toolbar_actions_bar_helper_.reset();
  BrowserWithTestWindowTest::TearDown();
}

void ToolbarActionsBarUnitTest::ActivateTab(int index) {
  ASSERT_NE(nullptr, browser()->tab_strip_model()->GetWebContentsAt(index));
  browser()->tab_strip_model()->ActivateTabAt(index, true);
  // Normally, the toolbar view would (indirectly) tell the toolbar actions
  // bar to update itself on tab change. Since this is a unittest, we have tell
  // it to.
  toolbar_actions_bar()->Update();
}

scoped_refptr<const extensions::Extension>
ToolbarActionsBarUnitTest::CreateAndAddExtension(
    const std::string& name,
    extensions::extension_action_test_util::ActionType action_type) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::extension_action_test_util::CreateActionExtension(
          name, action_type);
  extensions::ExtensionSystem::Get(profile())->extension_service()->
      AddExtension(extension.get());
  return extension;
}

void ToolbarActionsBarUnitTest::SetActionWantsToRunOnTab(
    ExtensionAction* action,
    content::WebContents* web_contents,
    bool wants_to_run) {
  action->SetIsVisible(SessionTabHelper::IdForTab(web_contents), wants_to_run);
  extensions::ExtensionActionAPI::Get(profile())->NotifyChange(
      action, web_contents, profile());
}

testing::AssertionResult ToolbarActionsBarUnitTest::VerifyToolbarOrder(
    const char* expected_names[],
    size_t total_size,
    size_t visible_count) {
  std::vector<ToolbarActionViewController*> toolbar_actions =
      toolbar_actions_bar()->toolbar_actions();
  // If the total size is wrong, we risk segfaulting by continuing. Abort now.
  if (total_size != toolbar_actions.size()) {
    return testing::AssertionFailure() << "Incorrect action count: expected " <<
        total_size << ", found " << toolbar_actions.size();
  }

  // Check that the ToolbarActionsBar matches the expected state.
  std::string error;
  for (size_t i = 0; i < total_size; ++i) {
    if (std::string(expected_names[i]) !=
            base::UTF16ToUTF8(toolbar_actions[i]->GetActionName())) {
      error += base::StringPrintf(
          "Incorrect action in bar at index %d: expected '%s', found '%s'.\n",
          static_cast<int>(i),
          expected_names[i],
          base::UTF16ToUTF8(toolbar_actions[i]->GetActionName()).c_str());
    }
  }
  size_t icon_count = toolbar_actions_bar()->GetIconCount();
  if (visible_count != icon_count)
    error += base::StringPrintf(
        "Incorrect visible count: expected %d, found %d",
        static_cast<int>(visible_count), static_cast<int>(icon_count));

  // Test that the (platform-specific) toolbar view matches the expected state.
  for (size_t i = 0; i < total_size; ++i) {
    std::string id = browser_action_test_util()->GetExtensionId(i);
    if (id != toolbar_actions[i]->GetId()) {
      error += base::StringPrintf(
          "Incorrect action in view at index %d: expected '%s', found '%s'.\n",
          static_cast<int>(i),
          toolbar_actions[i]->GetId().c_str(),
          id.c_str());
    }
  }
  size_t view_icon_count = browser_action_test_util()->VisibleBrowserActions();
  if (visible_count != view_icon_count)
    error += base::StringPrintf(
        "Incorrect visible count in view: expected %d, found %d",
        static_cast<int>(visible_count), static_cast<int>(view_icon_count));

  return error.empty() ? testing::AssertionSuccess() :
      testing::AssertionFailure() << error;
}

// A version of the ToolbarActionsBarUnitTest that enables the toolbar redesign.
class ToolbarActionsBarUnitTestWithSwitch : public ToolbarActionsBarUnitTest {
 public:
  ToolbarActionsBarUnitTestWithSwitch() {}
  ~ToolbarActionsBarUnitTestWithSwitch() override {}

 protected:
  void SetUp() override {
    redesign_switch_.reset(new extensions::FeatureSwitch::ScopedOverride(
        extensions::FeatureSwitch::extension_action_redesign(), true));
    ToolbarActionsBarUnitTest::SetUp();
  }
  void TearDown() override {
    redesign_switch_.reset();
    ToolbarActionsBarUnitTest::TearDown();
  }

 private:
  scoped_ptr<extensions::FeatureSwitch::ScopedOverride> redesign_switch_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarUnitTestWithSwitch);
};

TEST_F(ToolbarActionsBarUnitTest, BasicToolbarActionsBarTest) {
  // Add three extensions to the profile; this is the easiest way to have
  // toolbar actions.
  for (int i = 0; i < 3; ++i) {
    CreateAndAddExtension(
        base::StringPrintf("extension %d", i),
        extensions::extension_action_test_util::BROWSER_ACTION);
  }

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
  toolbar_model()->SetVisibleIconCount(2u);
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
  const char kExtension0[] = "extension 0";
  const char kExtension1[] = "extension 1";
  const char kExtension2[] = "extension 2";

  {
    // The order should start as 0, 1, 2.
    const char* expected_names[] = { kExtension0, kExtension1, kExtension2 };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // Drag 0 to be in the second spot; 1, 0, 2, within the same container.
    toolbar_actions_bar()->OnDragDrop(0, 1, ToolbarActionsBar::DRAG_TO_SAME);
    const char* expected_names[] = { kExtension1, kExtension0, kExtension2 };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // Drag 0 to be in the third spot, in the overflow container.
    // Order should be 1, 2, 0, and the icon count should reduce by 1.
    toolbar_actions_bar()->OnDragDrop(
        1, 2, ToolbarActionsBar::DRAG_TO_OVERFLOW);
    const char* expected_names[] = { kExtension1, kExtension2, kExtension0 };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
    // The model should also reflect the updated icon count.
    EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
    // Dragging 2 to the main container should work, even if its spot in the
    // "list" remains constant.
    // Order remains 1, 2, 0, but now we have 2 icons visible.
    toolbar_actions_bar()->OnDragDrop(1, 1, ToolbarActionsBar::DRAG_TO_MAIN);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
    // Similarly, dragging 2 to overflow, with the same "list" spot, should also
    // work. Order remains 1, 2, 0, but icon count goes back to 1.
    toolbar_actions_bar()->OnDragDrop(
        1, 1, ToolbarActionsBar::DRAG_TO_OVERFLOW);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
  }

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

// Test that toolbar actions can pop themselves out of overflow if they want to
// act on a given tab.
TEST_F(ToolbarActionsBarUnitTestWithSwitch, ActionsPopOutToAct) {
  // Add three extensions to the profile; this is the easiest way to have
  // toolbar actions.
  const char kBrowserAction[] = "browser action";
  const char kPageAction[] = "page action";
  const char kNoAction[] = "no action";

  CreateAndAddExtension(kBrowserAction,
                        extensions::extension_action_test_util::BROWSER_ACTION);
  scoped_refptr<const extensions::Extension> page_action =
      CreateAndAddExtension(
          kPageAction, extensions::extension_action_test_util::PAGE_ACTION);
  CreateAndAddExtension(kNoAction,
                        extensions::extension_action_test_util::NO_ACTION);

  {
    // We should start in the order of "browser action", "page action",
    // "no action" and have all actions visible.
    const char* expected_names[] = { kBrowserAction, kPageAction, kNoAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }

  // Shrink the bar to only show one action, and move the page action to the
  // end.
  toolbar_model()->SetVisibleIconCount(1);
  toolbar_model()->MoveExtensionIcon(page_action->id(), 2u);

  {
    // Quickly verify that the move/visible count worked.
    const char* expected_names[] = { kBrowserAction, kNoAction, kPageAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
  }

  // Create two tabs.
  AddTab(browser(), GURL("http://www.google.com/"));
  AddTab(browser(), GURL("http://www.youtube.com/"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  {
    // First, check the order for the first tab. Since we haven't changed
    // anything (i.e., no extensions want to act), this should be the same as we
    // left it: "browser action", "no action", "page action", with only one
    // visible.
    ActivateTab(0);
    const char* expected_names[] = { kBrowserAction, kNoAction, kPageAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
  }

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*page_action);
  ASSERT_TRUE(action);

  {
    // Make "page action" want to act.
    SetActionWantsToRunOnTab(action, web_contents, true);
    // This should result in "page action" being popped out of the overflow
    // menu.
    // This has two visible effects:
    // - page action should moved to the zero-index (left-most side of the bar).
    // - The visible count should increase by one (so page action is visible).
    const char* expected_names[] = { kPageAction, kBrowserAction, kNoAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // This should not have any effect on the second tab, which should still
    // have the original order and visible count.
    ActivateTab(1);
    const char* expected_names[] = { kBrowserAction, kNoAction, kPageAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
  }

 {
    // Switching back to the first tab should mean that actions that want to run
    // are re-popped out.
    ActivateTab(0);
    const char* expected_names[] = { kPageAction, kBrowserAction, kNoAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // Now, set the action to be hidden again, and notify of the change.
    SetActionWantsToRunOnTab(action, web_contents, false);
    // The order and visible count should return to normal (the page action
    // should move back to its original index in overflow).
    const char* expected_names[] = { kBrowserAction, kNoAction, kPageAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
  }

  {
    // Move page action to the second index and increase visible size to 2 (so
    // it's naturally visible).
    toolbar_model()->MoveExtensionIcon(page_action->id(), 1u);
    toolbar_model()->SetVisibleIconCount(2u);
    const char* expected_names[] = { kBrowserAction, kPageAction, kNoAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
    // Make the now-visible page action want to act.
    // Since the action is already visible, this should have no effect - the
    // order and visible count should remain unchanged.
    SetActionWantsToRunOnTab(action, web_contents, true);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // We should still be able to increase the size of the model, and to move
    // the page action.
    toolbar_model()->SetVisibleIconCount(3);
    toolbar_model()->MoveExtensionIcon(page_action->id(), 0u);
    const char* expected_names[] = { kPageAction, kBrowserAction, kNoAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
    // If we moved the page action, the move should remain in effect even after
    // the action no longer wants to act.
    SetActionWantsToRunOnTab(action, web_contents, false);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }

  {
    // Test the edge case of having no icons visible.
    toolbar_model()->SetVisibleIconCount(0);
    const char* expected_names[] = { kPageAction, kBrowserAction, kNoAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 0u));
    SetActionWantsToRunOnTab(action, web_contents, true);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
  }

  // Reset the action, and create another page action extension.
  SetActionWantsToRunOnTab(action, web_contents, false);
  const char kPageAction2[] = "page action2";
  scoped_refptr<const extensions::Extension> page_action2 =
      CreateAndAddExtension(
          kPageAction2, extensions::extension_action_test_util::PAGE_ACTION);

  {
    // The second page action should be added to the end, and no icons should
    // be visible.
    const char* expected_names[] =
        { kPageAction, kBrowserAction, kNoAction, kPageAction2 };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 4u, 0u));
  }

  {
    // Make both page actions want to run, with the second triggering first.
    SetActionWantsToRunOnTab(
        action_manager->GetExtensionAction(*page_action2), web_contents, true);
    SetActionWantsToRunOnTab(action, web_contents, true);

    // Even though the second page action triggered first, the order of actions
    // wanting to run should respect the normal order of actions.
    const char* expected_names[] =
        { kPageAction, kPageAction2, kBrowserAction, kNoAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 4u, 2u));
  }
}
