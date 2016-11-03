// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_with_badge_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/feature_switch.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/menu/menu_controller.h"

class PageActionImageViewInteractiveUITest : public ExtensionBrowserTest {
 protected:
  PageActionImageViewInteractiveUITest() {}
  ~PageActionImageViewInteractiveUITest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    // Testing page action-specific UI means we need to disable the redesign.
    disable_redesign_.reset(new extensions::FeatureSwitch::ScopedOverride(
        extensions::FeatureSwitch::extension_action_redesign(), false));
  }

 private:
  std::unique_ptr<extensions::FeatureSwitch::ScopedOverride> disable_redesign_;

  DISALLOW_COPY_AND_ASSIGN(PageActionImageViewInteractiveUITest);
};

// An ImageSkia source that will do nothing. We need this because we need a
// blank canvas at a certain size, and that can't be done by just using a null
// ImageSkia.
class BlankImageSource : public gfx::CanvasImageSource {
 public:
  explicit BlankImageSource(const gfx::Size& size)
      : gfx::CanvasImageSource(size, false) {}
  ~BlankImageSource() override {}

  void Draw(gfx::Canvas* canvas) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BlankImageSource);
};

void TestMenuIsOpenHelper(const base::Closure& quit_closure) {
  views::MenuController* menu_controller =
      views::MenuController::GetActiveInstance();

  // Using an ASSERT_TRUE here would make more sense, but would prevent the
  // |quit_closure| from running, which, since this is a menu run loop,
  // actually causes the test to time out, rather than fail cleanly.
  if (menu_controller)
    menu_controller->CancelAll();
  else
    ADD_FAILURE() << "No active menu when context menu should be open.";

  quit_closure.Run();
}

// Tests that right clicking a page action opens a context menu, and left
// clicking activates the action.
IN_PROC_BROWSER_TEST_F(PageActionImageViewInteractiveUITest,
                       TestBasicPageActionFunctions) {
  const extensions::Extension* extension = nullptr;
  // Load an extension that has a page action.
  {
    ExtensionTestMessageListener listener("ready", false);
    extension = LoadExtension(test_data_dir_.AppendASCII("api_test")
                                            .AppendASCII("page_action")
                                            .AppendASCII("simple"));
    ASSERT_TRUE(extension);
    // Ensure the extension is fully loaded.
    listener.WaitUntilSatisfied();
  }

  LocationBarView* location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();

  // There should be one and only one page action visible, and it should be
  // for the installed extension.
  // We need to use the browser->window->location bar flow (instead of just
  // using location_bar above) because of a bunch of private inheritance.
  LocationBarTesting* location_bar_testing =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  EXPECT_EQ(1, location_bar_testing->PageActionVisibleCount());
  PageActionWithBadgeView* page_action_with_badge_view =
      location_bar->GetPageActionView(
          extensions::ExtensionActionManager::Get(profile())->
              GetPageAction(*extension));
  ASSERT_TRUE(page_action_with_badge_view);
  PageActionImageView* page_action_view =
      page_action_with_badge_view->image_view();
  ASSERT_TRUE(page_action_view);
  EXPECT_TRUE(page_action_view->visible());

  // Compare the actual and expected icons.
  ExtensionAction* action = extensions::ExtensionActionManager::Get(profile())
                                ->GetPageAction(*extension);
  ExtensionActionIconFactory icon_factory(profile(), extension, action,
                                          nullptr);
  int tab_id = SessionTabHelper::IdForTab(
      browser()->tab_strip_model()->GetActiveWebContents());
  gfx::ImageSkia expected_icon = icon_factory.GetIcon(tab_id).AsImageSkia();

  // We need to resize the expected icon to be the size of the page action
  // button for comparison purposes.
  const gfx::Size size(ExtensionAction::ActionIconSize(),
                       ExtensionAction::ActionIconSize());
  gfx::ImageSkia bg(new BlankImageSource(size), size);
  expected_icon =
      gfx::ImageSkiaOperations::CreateSuperimposedImage(bg, expected_icon);
  SkBitmap expected_bitmap = expected_icon.GetRepresentation(1.0).sk_bitmap();
  SkBitmap actual_bitmap =
      page_action_view->GetImage().GetRepresentation(1.0).sk_bitmap();
  EXPECT_TRUE(gfx::BitmapsAreEqual(expected_bitmap, actual_bitmap));

  // Move the mouse over the center of the page action view.
  {
    base::RunLoop run_loop;
    gfx::Point page_action_center =
        ui_test_utils::GetCenterInScreenCoordinates(page_action_view);
    ui_controls::SendMouseMoveNotifyWhenDone(page_action_center.x(),
                                             page_action_center.y(),
                                             run_loop.QuitClosure());
    run_loop.Run();
  }

  // Right click on the mouse. This should open the menu.
  {
    base::RunLoop run_loop;
    // We use a helper function here because passing in quit closures directly
    // when running a menu loop doesn't play nicely.
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::RIGHT,
        ui_controls::UP | ui_controls::DOWN,
        base::Bind(&TestMenuIsOpenHelper, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // There shouldn't be an active menu anymore.
  EXPECT_FALSE(views::MenuController::GetActiveInstance());

  // The action should still want to run, and still be visible.
  EXPECT_EQ(1, location_bar_testing->PageActionVisibleCount());

  // Make sure the mouse is still over the center of the view (this is needed
  // on CrOS trybots).
  {
    base::RunLoop run_loop;
    gfx::Point page_action_center =
        ui_test_utils::GetCenterInScreenCoordinates(page_action_view);
    ui_controls::SendMouseMoveNotifyWhenDone(page_action_center.x(),
                                             page_action_center.y(),
                                             run_loop.QuitClosure());
    run_loop.Run();
  }

  // Left click on the mouse. This should execute the page action.
  {
    base::RunLoop run_loop;
    ExtensionTestMessageListener listener("clicked", false);
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT,
        ui_controls::UP | ui_controls::DOWN,
        run_loop.QuitClosure());
    run_loop.Run();
    listener.WaitUntilSatisfied();
  }

  // Since the action ran, it should no longer be visible.
  EXPECT_EQ(0, location_bar_testing->PageActionVisibleCount());
}
