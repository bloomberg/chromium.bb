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

namespace {

// Verifies that the toolbar order matches for the given |actions_bar|. If the
// order matches, the return value is empty; otherwise, it contains the error.
std::string VerifyToolbarOrderForBar(
    const ToolbarActionsBar* actions_bar,
    BrowserActionTestUtil* browser_action_test_util,
    const char* expected_names[],
    size_t total_size,
    size_t visible_count) {
  const std::vector<ToolbarActionViewController*>& toolbar_actions =
      actions_bar->toolbar_actions();
  // If the total size is wrong, we risk segfaulting by continuing. Abort now.
  if (total_size != toolbar_actions.size()) {
    return base::StringPrintf("Incorrect action count: expected %d, found %d",
                              static_cast<int>(total_size),
                              static_cast<int>(toolbar_actions.size()));
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
  size_t icon_count = actions_bar->GetIconCount();
  if (visible_count != icon_count)
    error += base::StringPrintf(
        "Incorrect visible count: expected %d, found %d.\n",
        static_cast<int>(visible_count), static_cast<int>(icon_count));

  // Test that the (platform-specific) toolbar view matches the expected state.
  for (size_t i = 0; i < total_size; ++i) {
    std::string id = browser_action_test_util->GetExtensionId(i);
    if (id != toolbar_actions[i]->GetId()) {
      error += base::StringPrintf(
          "Incorrect action in view at index %d: expected '%s', found '%s'.\n",
          static_cast<int>(i),
          toolbar_actions[i]->GetId().c_str(),
          id.c_str());
    }
  }
  size_t view_icon_count = browser_action_test_util->VisibleBrowserActions();
  if (visible_count != view_icon_count)
    error += base::StringPrintf(
        "Incorrect visible count in view: expected %d, found %d.\n",
        static_cast<int>(visible_count), static_cast<int>(view_icon_count));

  return error;
}

}  // namespace

// A cross-platform unit test for the ToolbarActionsBar that uses the
// TestToolbarActionsBarHelper to create the platform-specific containers.
// TODO(devlin): Since this *does* use the real platform containers, in theory,
// we can move all the BrowserActionsBarBrowserTests to be unittests. See about
// doing this.
class ToolbarActionsBarUnitTest : public BrowserWithTestWindowTest {
 public:
  ToolbarActionsBarUnitTest() : toolbar_model_(nullptr), use_redesign_(false) {}

  // A constructor to allow subclasses to override the redesign value.
  explicit ToolbarActionsBarUnitTest(bool use_redesign)
      : toolbar_model_(nullptr),
        use_redesign_(use_redesign) {}

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
    return main_bar_helper_->GetToolbarActionsBar();
  }
  ToolbarActionsBar* overflow_bar() {
    return overflow_bar_helper_->GetToolbarActionsBar();
  }
  extensions::ExtensionToolbarModel* toolbar_model() {
    return toolbar_model_;
  }
  BrowserActionTestUtil* browser_action_test_util() {
    return browser_action_test_util_.get();
  }
  BrowserActionTestUtil* overflow_browser_action_test_util() {
    return overflow_browser_action_test_util_.get();
  }

 private:
  // The test helper that owns the ToolbarActionsBar and the platform-specific
  // view for it.
  scoped_ptr<TestToolbarActionsBarHelper> main_bar_helper_;

  // The test helper for the overflow bar; only non-null if |use_redesign| is
  // true.
  scoped_ptr<TestToolbarActionsBarHelper> overflow_bar_helper_;

  // The associated ExtensionToolbarModel (owned by the keyed service setup).
  extensions::ExtensionToolbarModel* toolbar_model_;

  // A BrowserActionTestUtil object constructed with the associated
  // ToolbarActionsBar.
  scoped_ptr<BrowserActionTestUtil> browser_action_test_util_;

  // The overflow container's BrowserActionTestUtil (only non-null if
  // |use_redesign| is true).
  scoped_ptr<BrowserActionTestUtil> overflow_browser_action_test_util_;

  // True if the extension action redesign switch should be enabled.
  bool use_redesign_;

  scoped_ptr<extensions::FeatureSwitch::ScopedOverride> redesign_switch_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarUnitTest);
};

void ToolbarActionsBarUnitTest::SetUp() {
  if (use_redesign_) {
    redesign_switch_.reset(new extensions::FeatureSwitch::ScopedOverride(
        extensions::FeatureSwitch::extension_action_redesign(), true));
  }

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

  main_bar_helper_ = TestToolbarActionsBarHelper::Create(browser(), nullptr);

  ToolbarActionsBar::disable_animations_for_testing_ = true;
  ToolbarActionsBar::set_send_overflowed_action_changes_for_testing(false);
  browser_action_test_util_.reset(
      new BrowserActionTestUtil(browser(),
                                toolbar_actions_bar()->delegate_for_test()));

  if (use_redesign_) {
    overflow_bar_helper_ =
        TestToolbarActionsBarHelper::Create(browser(), main_bar_helper_.get());
    overflow_browser_action_test_util_.reset(
        new BrowserActionTestUtil(browser(),
                                  overflow_bar()->delegate_for_test()));
  }
}

void ToolbarActionsBarUnitTest::TearDown() {
  // Since the profile gets destroyed in BrowserWithTestWindowTest::TearDown(),
  // we need to delete this now.
  overflow_bar_helper_.reset();
  main_bar_helper_.reset();
  ToolbarActionsBar::disable_animations_for_testing_ = false;
  redesign_switch_.reset();
  BrowserWithTestWindowTest::TearDown();
}

void ToolbarActionsBarUnitTest::ActivateTab(int index) {
  ASSERT_NE(nullptr, browser()->tab_strip_model()->GetWebContentsAt(index));
  browser()->tab_strip_model()->ActivateTabAt(index, true);
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
  std::string main_bar_error =
      VerifyToolbarOrderForBar(toolbar_actions_bar(),
                               browser_action_test_util(),
                               expected_names,
                               total_size,
                               visible_count);
  std::string overflow_bar_error;
  if (use_redesign_) {
    overflow_bar_error =
        VerifyToolbarOrderForBar(overflow_bar(),
                                 overflow_browser_action_test_util(),
                                 expected_names,
                                 total_size,
                                 total_size - visible_count);

  }

  return main_bar_error.empty() && overflow_bar_error.empty() ?
      testing::AssertionSuccess() :
      testing::AssertionFailure() << "main bar error:\n" << main_bar_error <<
          "overflow bar error:\n" << overflow_bar_error;
}

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

class ToolbarActionsBarPopOutUnitTest
     : public ToolbarActionsBarUnitTest {
 public:
  ToolbarActionsBarPopOutUnitTest() : ToolbarActionsBarUnitTest(true) {}

 protected:
  void SetUp() override {
    ToolbarActionsBar::set_pop_out_actions_to_run_for_testing(true);
    ToolbarActionsBarUnitTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarPopOutUnitTest);
};

// Test that toolbar actions can pop themselves out of overflow if they want to
// act on a given tab.
TEST_F(ToolbarActionsBarPopOutUnitTest, ActionsPopOutToAct) {
  // Add three extensions to the profile; this is the easiest way to have
  // toolbar actions.
  const char kBrowserAction[] = "browser action";
  const char kPageAction[] = "page action";
  const char kSynthetic[] = "synthetic";  // This has a generated action icon.

  CreateAndAddExtension(kBrowserAction,
                        extensions::extension_action_test_util::BROWSER_ACTION);
  scoped_refptr<const extensions::Extension> page_action =
      CreateAndAddExtension(
          kPageAction, extensions::extension_action_test_util::PAGE_ACTION);
  CreateAndAddExtension(kSynthetic,
                        extensions::extension_action_test_util::NO_ACTION);

  {
    // We should start in the order of "browser action", "page action",
    // "synthetic" and have all actions visible.
    const char* expected_names[] = { kBrowserAction, kPageAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }

  // Shrink the bar to only show one action, and move the page action to the
  // end.
  toolbar_model()->SetVisibleIconCount(1);
  toolbar_model()->MoveExtensionIcon(page_action->id(), 2u);

  {
    // Quickly verify that the move/visible count worked.
    const char* expected_names[] = { kBrowserAction, kSynthetic, kPageAction };
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
    // left it: "browser action", "synthetic", "page action", with only one
    // visible.
    ActivateTab(0);
    const char* expected_names[] = { kBrowserAction, kSynthetic, kPageAction };
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
    const char* expected_names[] = { kPageAction, kBrowserAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // This should not have any effect on the second tab, which should still
    // have the original order and visible count.
    ActivateTab(1);
    const char* expected_names[] = { kBrowserAction, kSynthetic, kPageAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
  }

 {
    // Switching back to the first tab should mean that actions that want to run
    // are re-popped out.
    ActivateTab(0);
    const char* expected_names[] = { kPageAction, kBrowserAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // Now, set the action to be hidden again, and notify of the change.
    SetActionWantsToRunOnTab(action, web_contents, false);
    // The order and visible count should return to normal (the page action
    // should move back to its original index in overflow).
    const char* expected_names[] = { kBrowserAction, kSynthetic, kPageAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
  }

  {
    // Move page action to the second index and increase visible size to 2 (so
    // it's naturally visible).
    toolbar_model()->MoveExtensionIcon(page_action->id(), 1u);
    toolbar_model()->SetVisibleIconCount(2u);
    const char* expected_names[] = { kBrowserAction, kPageAction, kSynthetic };
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
    const char* expected_names[] = { kPageAction, kBrowserAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
    // If we moved the page action, the move should remain in effect even after
    // the action no longer wants to act.
    SetActionWantsToRunOnTab(action, web_contents, false);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }

  {
    // Test the edge case of having no icons visible.
    toolbar_model()->SetVisibleIconCount(0);
    const char* expected_names[] = { kPageAction, kBrowserAction, kSynthetic };
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
  // The new extension was installed visible. Move it to the last index, and
  // adjust the visible count.
  toolbar_model()->MoveExtensionIcon(page_action2->id(), 3u);
  toolbar_model()->SetVisibleIconCount(0);

  {
    // The second page action should be added to the end, and no icons should
    // be visible.
    const char* expected_names[] =
        { kPageAction, kBrowserAction, kSynthetic, kPageAction2 };
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
        { kPageAction, kPageAction2, kBrowserAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 4u, 2u));
  }
}

TEST_F(ToolbarActionsBarPopOutUnitTest, AdjustingActionsThatWantToAct) {
  // Add three extensions to the profile; this is the easiest way to have
  // toolbar actions.
  const char kBrowserAction[] = "browser action";
  const char kPageAction[] = "page action";
  const char kSynthetic[] = "synthetic";  // This has a generated action icon.

  CreateAndAddExtension(kBrowserAction,
                        extensions::extension_action_test_util::BROWSER_ACTION);
  scoped_refptr<const extensions::Extension> page_action =
      CreateAndAddExtension(
          kPageAction, extensions::extension_action_test_util::PAGE_ACTION);
  CreateAndAddExtension(kSynthetic,
                        extensions::extension_action_test_util::NO_ACTION);

  // Move the page action to the second index and reduce the visible count to 1
  // so that the page action is hidden (and can pop out when it needs to act).
  toolbar_model()->SetVisibleIconCount(1);
  toolbar_model()->MoveExtensionIcon(page_action->id(), 1u);

  // Create a tab.
  AddTab(browser(), GURL("http://www.google.com/"));
  AddTab(browser(), GURL("http://www.youtube.com/"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*page_action);
  ASSERT_TRUE(action);

  {
    // Make the page action pop out, which causes the visible count for tab to
    // become 2, with the page action at the front.
    SetActionWantsToRunOnTab(action, web_contents, true);
    const char* expected_names[] = { kPageAction, kBrowserAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // Shrink the toolbar count. To the user, this is hiding "browser action",
    // so that's the effect it should have (browser action should be hidden from
    // all windows).
    toolbar_actions_bar()->OnResizeComplete(
        toolbar_actions_bar()->IconCountToWidth(1u));
    const char* expected_names[] = { kPageAction, kBrowserAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
    // Once the action no longer wants to run, "page action" should go back to
    // its normal spot, and visible count goes to zero.
    SetActionWantsToRunOnTab(action, web_contents, false);
    const char* expected_names2[] = { kBrowserAction, kPageAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names2, 3u, 0u));
  }

  {
    // Make the action want to run again - it should pop out, and be the only
    // action visible on the tab.
    SetActionWantsToRunOnTab(action, web_contents, true);
    const char* expected_names[] = { kPageAction, kBrowserAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
    // Set the visible icon count for that tab to 2. This uncovers one action,
    // so the base visible count should be 1, and the order should still be
    // "page action", "browser action", "synthetic".
    toolbar_actions_bar()->OnResizeComplete(
        toolbar_actions_bar()->IconCountToWidth(2u));
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
    // Next, grow the item order to 3 for the tab. This uncovers the "synthetic"
    // extension. This is interesting, because for the same action to be
    // uncovered on other tabs, the underlying order (which was previously
    // "browser action", "page action", "synthetic") has to chage (to be
    // "browser action", "synthetic", "page action"). If we don't make this
    // change, we uncover "synthetic" here, but in other windows, "page action"
    // is uncovered (which is weird). Ensure that the change that was visible
    // to the user (uncovering "synthetic") is the one that happens to the
    // underlying model.
    toolbar_actions_bar()->OnResizeComplete(
        toolbar_actions_bar()->IconCountToWidth(3u));
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
    // So when "page action" finishes, it should go back to overflow, leaving
    // the other two visible.
    SetActionWantsToRunOnTab(action, web_contents, false);
    const char* expected_names2[] = { kBrowserAction, kSynthetic, kPageAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names2, 3u, 2u));
  }

  {
    // Next, test that moving an action that was popped out for overflow pins
    // the action to the new spot. Since the action originated in overflow, this
    // also causes the base visible count to increase.
    // Set the action to be visible.
    SetActionWantsToRunOnTab(action, web_contents, true);
    const char* expected_names[] = { kPageAction, kBrowserAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
    // Move the popped out "page action" extension to the first index.
    toolbar_actions_bar()->OnDragDrop(0, 1, ToolbarActionsBar::DRAG_TO_SAME);
    const char* expected_names2[] = { kBrowserAction, kPageAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names2, 3u, 3u));
    // Since this pinned "page action", the order stays the same after the run.
    SetActionWantsToRunOnTab(action, web_contents, false);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names2, 3u, 3u));
  }

  {
    // Move "page action" back to overflow.
    toolbar_actions_bar()->OnDragDrop(1, 2, ToolbarActionsBar::DRAG_TO_SAME);
    toolbar_actions_bar()->OnResizeComplete(
        toolbar_actions_bar()->IconCountToWidth(2u));

    // Test moving a popped out extension to the overflow menu; this should have
    // no effect on the base visible count.
    SetActionWantsToRunOnTab(action, web_contents, true);
    const char* expected_names[] = { kPageAction, kBrowserAction, kSynthetic };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
    // Move "page action" to the overflow menu.
    toolbar_actions_bar()->OnDragDrop(
        0, 2, ToolbarActionsBar::DRAG_TO_OVERFLOW);
    const char* expected_names2[] = { kBrowserAction, kSynthetic, kPageAction };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names2, 3u, 2u));
    SetActionWantsToRunOnTab(action, web_contents, false);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names2, 3u, 2u));
  }
}
