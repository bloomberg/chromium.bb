// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_testing_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"

using extensions::Extension;

class BrowserActionsContainerTest : public ExtensionBrowserTest {
 public:
  BrowserActionsContainerTest() {
  }
  virtual ~BrowserActionsContainerTest() {}

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    BrowserActionsContainer::disable_animations_during_testing_ = true;
    ExtensionBrowserTest::SetUpCommandLine(command_line);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionBrowserTest::SetUpOnMainThread();
    browser_actions_bar_.reset(new BrowserActionTestUtil(browser()));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    BrowserActionsContainer::disable_animations_during_testing_ = false;
  }

  BrowserActionTestUtil* browser_actions_bar() {
    return browser_actions_bar_.get();
  }

 private:
  scoped_ptr<BrowserActionTestUtil> browser_actions_bar_;
};

// Test the basic functionality.
// http://crbug.com/120770
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(BrowserActionsContainerTest, DISABLED_Basic) {
#else
IN_PROC_BROWSER_TEST_F(BrowserActionsContainerTest, Basic) {
#endif
  // Load an extension with no browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("none")));
  // This extension should not be in the model (has no browser action).
  EXPECT_EQ(0, browser_actions_bar()->NumberOfBrowserActions());

  // Load an extension with a browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(0));


  // Unload the extension.
  std::string id = browser_actions_bar()->GetExtensionId(0);
  UnloadExtension(id);
  EXPECT_EQ(0, browser_actions_bar()->NumberOfBrowserActions());
}

// Test moving various browser actions. This is not to check the logic of the
// move (that's in the toolbar model tests), but just to check our ui.
IN_PROC_BROWSER_TEST_F(BrowserActionsContainerTest,
                       MoveBrowserActions) {
  // Load three extensions with browser actions.
  const extensions::Extension* extension_a =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                                  .AppendASCII("browser_action")
                                  .AppendASCII("basics"));
  ASSERT_TRUE(extension_a);
  const extensions::Extension* extension_b =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                                  .AppendASCII("browser_action")
                                  .AppendASCII("add_popup"));
  ASSERT_TRUE(extension_b);
  const extensions::Extension* extension_c =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                                  .AppendASCII("browser_action")
                                  .AppendASCII("remove_popup"));
  ASSERT_TRUE(extension_c);

  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());

  extensions::ExtensionToolbarModel* model =
      extensions::ExtensionToolbarModel::Get(profile());
  ASSERT_TRUE(model);

  // Order is now A B C.
  EXPECT_EQ(extension_a->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_b->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_c->id(), browser_actions_bar()->GetExtensionId(2));

  // Move C to first position. Order is C A B.
  model->MoveExtensionIcon(extension_c, 0);
  EXPECT_EQ(extension_c->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_a->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_b->id(), browser_actions_bar()->GetExtensionId(2));

  // Move B to third position. Order is still C A B.
  model->MoveExtensionIcon(extension_b, 2);
  EXPECT_EQ(extension_c->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_a->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_b->id(), browser_actions_bar()->GetExtensionId(2));

  // Move B to middle position. Order is C B A.
  model->MoveExtensionIcon(extension_b, 1);
  EXPECT_EQ(extension_c->id(), browser_actions_bar()->GetExtensionId(0));
  EXPECT_EQ(extension_b->id(), browser_actions_bar()->GetExtensionId(1));
  EXPECT_EQ(extension_a->id(), browser_actions_bar()->GetExtensionId(2));
}

IN_PROC_BROWSER_TEST_F(BrowserActionsContainerTest, Visibility) {
  // Load extension A (contains browser action).
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(0));
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  std::string idA = browser_actions_bar()->GetExtensionId(0);

  // Load extension B (contains browser action).
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("add_popup")));
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(0));
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  std::string idB = browser_actions_bar()->GetExtensionId(1);

  EXPECT_NE(idA, idB);

  // Load extension C (contains browser action).
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("remove_popup")));
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(2));
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  std::string idC = browser_actions_bar()->GetExtensionId(2);

  // Change container to show only one action, rest in overflow: A, [B, C].
  browser_actions_bar()->SetIconVisibilityCount(1);
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());

  // Disable extension A (should disappear). State becomes: B [C].
  DisableExtension(idA);
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(idB, browser_actions_bar()->GetExtensionId(0));

  // Enable A again. A should get its spot in the same location and the bar
  // should not grow (chevron is showing). For details: http://crbug.com/35349.
  // State becomes: A, [B, C].
  EnableExtension(idA);
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(idA, browser_actions_bar()->GetExtensionId(0));

  // Disable C (in overflow). State becomes: A, [B].
  DisableExtension(idC);
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(idA, browser_actions_bar()->GetExtensionId(0));

  // Enable C again. State becomes: A, [B, C].
  EnableExtension(idC);
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(idA, browser_actions_bar()->GetExtensionId(0));

  // Now we have 3 extensions. Make sure they are all visible. State: A, B, C.
  browser_actions_bar()->SetIconVisibilityCount(3);
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());

  // Disable extension A (should disappear). State becomes: B, C.
  DisableExtension(idA);
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(idB, browser_actions_bar()->GetExtensionId(0));

  // Disable extension B (should disappear). State becomes: C.
  DisableExtension(idB);
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(idC, browser_actions_bar()->GetExtensionId(0));

  // Enable B. State becomes: B, C.
  EnableExtension(idB);
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(idB, browser_actions_bar()->GetExtensionId(0));

  // Enable A. State becomes: A, B, C.
  EnableExtension(idA);
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(idA, browser_actions_bar()->GetExtensionId(0));
}

IN_PROC_BROWSER_TEST_F(BrowserActionsContainerTest, ForceHide) {
  // Load extension A (contains browser action).
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(0));
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  std::string idA = browser_actions_bar()->GetExtensionId(0);

  // Force hide this browser action.
  extensions::ExtensionActionAPI::SetBrowserActionVisibility(
      extensions::ExtensionPrefs::Get(browser()->profile()), idA, false);
  EXPECT_EQ(0, browser_actions_bar()->VisibleBrowserActions());
}

// Test that the BrowserActionsContainer responds correctly when the underlying
// model enters highlight mode, and that browser actions are undraggable in
// highlight mode. (Highlight mode itself it tested more thoroughly in the
// ExtensionToolbarModel browsertests).
IN_PROC_BROWSER_TEST_F(BrowserActionsContainerTest, HighlightMode) {
  // Load three extensions with browser actions.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));
  std::string id_a = browser_actions_bar()->GetExtensionId(0);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("add_popup")));
  std::string id_b = browser_actions_bar()->GetExtensionId(1);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("remove_popup")));

  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());

  BrowserActionsContainer* container = browser()
                                           ->window()
                                           ->GetBrowserWindowTesting()
                                           ->GetToolbarView()
                                           ->browser_actions();

  // Currently, dragging should be enabled.
  BrowserActionView* action_view = container->GetBrowserActionViewAt(0);
  ASSERT_TRUE(action_view);
  gfx::Point point(action_view->x(), action_view->y());
  EXPECT_TRUE(container->CanStartDragForView(action_view, point, point));

  extensions::ExtensionToolbarModel* model =
      extensions::ExtensionToolbarModel::Get(profile());

  extensions::ExtensionIdList extension_ids;
  extension_ids.push_back(id_a);
  extension_ids.push_back(id_b);
  model->HighlightExtensions(extension_ids);

  // Only two browser actions should be visible.
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());

  // We shouldn't be able to drag in highlight mode.
  action_view = container->GetBrowserActionViewAt(0);
  EXPECT_FALSE(container->CanStartDragForView(action_view, point, point));

  // We should go back to normal after leaving highlight mode.
  model->StopHighlighting();
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  action_view = container->GetBrowserActionViewAt(0);
  EXPECT_TRUE(container->CanStartDragForView(action_view, point, point));
}

// Test the behavior of the overflow container for Extension Actions.
class BrowserActionsContainerOverflowTest : public BrowserActionsContainerTest {
 public:
  BrowserActionsContainerOverflowTest() : main_bar_(NULL), model_(NULL) {
  }
  virtual ~BrowserActionsContainerOverflowTest() {
  }

 protected:
  // Returns true if the order of the BrowserActionViews in |main_bar_|
  // and |overflow_bar_| match.
  bool ViewOrdersMatch();

  // Returns Success if the visible count matches |expected_visible|. This means
  // that the number of visible browser actions in |main_bar_| is
  // |expected_visible| and shows the first icons, and that the overflow bar
  // shows all (and only) the remainder.
  testing::AssertionResult VerifyVisibleCount(size_t expected_visible);

  // Accessors.
  BrowserActionsContainer* main_bar() { return main_bar_; }
  BrowserActionsContainer* overflow_bar() { return overflow_bar_.get(); }
  extensions::ExtensionToolbarModel* model() { return model_; }

 private:
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void TearDownOnMainThread() OVERRIDE;

  // The main BrowserActionsContainer (owned by the browser view).
  BrowserActionsContainer* main_bar_;

  // The overflow BrowserActionsContainer. We manufacture this so that we don't
  // have to open the wrench menu.
  scoped_ptr<BrowserActionsContainer> overflow_bar_;

  // The associated toolbar model.
  extensions::ExtensionToolbarModel* model_;

  // Enable the feature redesign switch.
  scoped_ptr<extensions::FeatureSwitch::ScopedOverride> enable_redesign_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsContainerOverflowTest);
};

void BrowserActionsContainerOverflowTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  BrowserActionsContainerTest::SetUpCommandLine(command_line);
  enable_redesign_.reset(new extensions::FeatureSwitch::ScopedOverride(
      extensions::FeatureSwitch::extension_action_redesign(),
      true));
}

void BrowserActionsContainerOverflowTest::SetUpOnMainThread() {
  BrowserActionsContainerTest::SetUpOnMainThread();
  main_bar_ = BrowserView::GetBrowserViewForBrowser(browser())
                  ->toolbar()->browser_actions();
  overflow_bar_.reset(new BrowserActionsContainer(browser(), NULL, main_bar_));
  overflow_bar_->set_owned_by_client();
  model_ = extensions::ExtensionToolbarModel::Get(profile());
}

void BrowserActionsContainerOverflowTest::TearDownOnMainThread() {
  overflow_bar_.reset();
  enable_redesign_.reset();
  BrowserActionsContainerTest::TearDownOnMainThread();
}

bool BrowserActionsContainerOverflowTest::ViewOrdersMatch() {
  if (main_bar_->num_browser_actions() !=
      overflow_bar_->num_browser_actions())
    return false;
  for (size_t i = 0; i < main_bar_->num_browser_actions(); ++i) {
    if (main_bar_->GetBrowserActionViewAt(i)->extension() !=
        overflow_bar_->GetBrowserActionViewAt(i)->extension())
      return false;
  }
  return true;
}

testing::AssertionResult
BrowserActionsContainerOverflowTest::VerifyVisibleCount(
    size_t expected_visible) {
  // Views order should always match (as it is based directly off the model).
  if (!ViewOrdersMatch())
    return testing::AssertionFailure() << "View orders don't match";

  // Loop through and check each browser action for proper visibility (which
  // implicitly also guarantees that the proper number are visible).
  for (size_t i = 0; i < overflow_bar_->num_browser_actions(); ++i) {
    bool visible = i < expected_visible;
    if (main_bar_->GetBrowserActionViewAt(i)->visible() != visible) {
      return testing::AssertionFailure() << "Index " << i <<
          " has improper visibility in main: " << !visible;
    }
    if (overflow_bar_->GetBrowserActionViewAt(i)->visible() == visible) {
      return testing::AssertionFailure() << "Index " << i <<
          " has improper visibility in overflow: " << visible;
    }
  }
  return testing::AssertionSuccess();
}

// Test the basic functionality of the BrowserActionsContainer in overflow mode.
IN_PROC_BROWSER_TEST_F(BrowserActionsContainerOverflowTest,
                       TestBasicActionOverflow) {
  // Load three extensions with browser actions.
  // TODO(devlin): Make a method to load these, and generate them rather than
  // using files.
  base::FilePath test_data_path =
      test_data_dir_.AppendASCII("api_test").AppendASCII("browser_action");
  const extensions::Extension* extension_a =
      LoadExtension(test_data_path.AppendASCII("basics"));
  const extensions::Extension* extension_b =
      LoadExtension(test_data_path.AppendASCII("add_popup"));
  const extensions::Extension* extension_c =
      LoadExtension(test_data_path.AppendASCII("remove_popup"));

  // Since the overflow bar isn't attached to a view, we have to kick it in
  // order to retrigger layout each time we change the number of icons in the
  // bar.
  overflow_bar()->Layout();

  // Sanity checks:
  // All extensions loaded.
  ASSERT_TRUE(extension_a);
  ASSERT_TRUE(extension_b);
  ASSERT_TRUE(extension_c);

  // All actions are showing, and are in the installation order.
  EXPECT_EQ(-1, model()->GetVisibleIconCount());
  ASSERT_EQ(3u, main_bar()->num_browser_actions());
  EXPECT_EQ(extension_a, main_bar()->GetBrowserActionViewAt(0)->extension());
  EXPECT_EQ(extension_b, main_bar()->GetBrowserActionViewAt(1)->extension());
  EXPECT_EQ(extension_c, main_bar()->GetBrowserActionViewAt(2)->extension());
  EXPECT_TRUE(VerifyVisibleCount(3u));

  // Reduce the visible count to 2. Order should be unchanged (A B C), but
  // only A and B should be visible on the main bar.
  model()->SetVisibleIconCountForTest(2u);
  overflow_bar()->Layout();  // Kick.
  EXPECT_EQ(extension_a, main_bar()->GetBrowserActionViewAt(0)->extension());
  EXPECT_EQ(extension_b, main_bar()->GetBrowserActionViewAt(1)->extension());
  EXPECT_EQ(extension_c, main_bar()->GetBrowserActionViewAt(2)->extension());
  EXPECT_TRUE(VerifyVisibleCount(2u));

  // Move extension C to the first position. Order should now be C A B, with
  // C and A visible in the main bar.
  model()->MoveExtensionIcon(extension_c, 0);
  overflow_bar()->Layout();  // Kick.
  EXPECT_EQ(extension_c, main_bar()->GetBrowserActionViewAt(0)->extension());
  EXPECT_EQ(extension_a, main_bar()->GetBrowserActionViewAt(1)->extension());
  EXPECT_EQ(extension_b, main_bar()->GetBrowserActionViewAt(2)->extension());
  EXPECT_TRUE(VerifyVisibleCount(2u));

  // Hide action A. This results in it being sent to overflow, and reducing the
  // visible size to 1, so the order should be C A B, with only C visible in the
  // main bar.
  extensions::ExtensionActionAPI::SetBrowserActionVisibility(
      extensions::ExtensionPrefs::Get(profile()),
      extension_a->id(),
      false);
  overflow_bar()->Layout();  // Kick.
  EXPECT_EQ(extension_c, main_bar()->GetBrowserActionViewAt(0)->extension());
  EXPECT_EQ(extension_a, main_bar()->GetBrowserActionViewAt(1)->extension());
  EXPECT_EQ(extension_b, main_bar()->GetBrowserActionViewAt(2)->extension());
  EXPECT_TRUE(VerifyVisibleCount(1u));
}
