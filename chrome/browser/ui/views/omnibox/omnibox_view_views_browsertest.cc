// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/test/ui_controls.h"

#if defined(USE_AURA)
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_processor.h"
#endif // defined(USE_AURA)

class OmniboxViewViewsTest : public InProcessBrowserTest {
 protected:
  OmniboxViewViewsTest() {}

  static void GetOmniboxViewForBrowser(const Browser* browser,
                                       OmniboxView** omnibox_view) {
    BrowserWindow* window = browser->window();
    ASSERT_TRUE(window);
    LocationBar* location_bar = window->GetLocationBar();
    ASSERT_TRUE(location_bar);
    *omnibox_view = location_bar->GetOmniboxView();
    ASSERT_TRUE(*omnibox_view);
  }

  // Move the mouse to the center of the browser window and left-click.
  void ClickBrowserWindowCenter() {
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        BrowserView::GetBrowserViewForBrowser(
            browser())->GetBoundsInScreen().CenterPoint()));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                                   ui_controls::DOWN));
    ASSERT_TRUE(
        ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  }

  // Press and release the mouse at the specified locations.  If
  // |release_offset| differs from |press_offset|, the mouse will be moved
  // between the press and release.
  void Click(ui_controls::MouseButton button,
             const gfx::Point& press_location,
             const gfx::Point& release_location) {
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(press_location));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(button, ui_controls::DOWN));

    if (press_location != release_location)
      ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(release_location));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(button, ui_controls::UP));
  }

#if defined(USE_AURA)
  // Tap the center of the browser window.
  void TapBrowserWindowCenter() {
    gfx::Point center = BrowserView::GetBrowserViewForBrowser(
        browser())->GetBoundsInScreen().CenterPoint();
    aura::test::EventGenerator generator(browser()->window()->
        GetNativeWindow()->GetRootWindow());
    generator.GestureTapAt(center);
  }

  // Touch down and release at the specified locations.
  void Tap(const gfx::Point& press_location,
           const gfx::Point& release_location) {
    ui::EventProcessor* dispatcher =
        browser()->window()->GetNativeWindow()->GetHost()->event_processor();

    ui::TouchEvent press(ui::ET_TOUCH_PRESSED, press_location,
                         5, base::TimeDelta::FromMilliseconds(0));
    ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&press);
    ASSERT_FALSE(details.dispatcher_destroyed);

    ui::TouchEvent release(ui::ET_TOUCH_RELEASED, release_location,
                           5, base::TimeDelta::FromMilliseconds(50));
    details = dispatcher->OnEventFromSource(&release);
    ASSERT_FALSE(details.dispatcher_destroyed);
  }
#endif // defined(USE_AURA)

 private:
  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    chrome::FocusLocationBar(browser());
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  }

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViewsTest);
};

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, PasteAndGoDoesNotLeavePopupOpen) {
  OmniboxView* view = browser()->window()->GetLocationBar()->GetOmniboxView();
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  // Put an URL on the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(
        ui::Clipboard::GetForCurrentThread(), ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteURL(base::ASCIIToUTF16("http://www.example.com/"));
  }

  // Paste and go.
  omnibox_view_views->ExecuteCommand(IDS_PASTE_AND_GO, ui::EF_NONE);

  // The popup should not be open.
  EXPECT_FALSE(view->model()->popup_model()->IsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, SelectAllOnClick) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  omnibox_view->SetUserText(base::ASCIIToUTF16("http://www.google.com/"));

  // Take the focus away from the omnibox.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Clicking in the omnibox should take focus and select all text.
  const gfx::Rect omnibox_bounds = BrowserView::GetBrowserViewForBrowser(
        browser())->GetViewByID(VIEW_ID_OMNIBOX)->GetBoundsInScreen();
  const gfx::Point click_location = omnibox_bounds.CenterPoint();
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Clicking in another view should clear focus and the selection.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Clicking in the omnibox again should take focus and select all text again.
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Clicking another omnibox spot should keep focus but clear the selection.
  omnibox_view->SelectAll(false);
  const gfx::Point click2_location = omnibox_bounds.origin() +
      gfx::Vector2d(omnibox_bounds.width() / 4, omnibox_bounds.height() / 4);
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click2_location, click2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Take the focus away and click in the omnibox again, but drag a bit before
  // releasing.  We should focus the omnibox but not select all of its text.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click_location, click2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Middle-clicking should not be handled by the omnibox.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::MIDDLE,
                                click_location, click_location));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
}

#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, SelectAllOnTap) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  omnibox_view->SetUserText(base::ASCIIToUTF16("http://www.google.com/"));

  // Take the focus away from the omnibox.
  ASSERT_NO_FATAL_FAILURE(TapBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Tapping in the omnibox should take focus and select all text.
  const gfx::Rect omnibox_bounds = BrowserView::GetBrowserViewForBrowser(
      browser())->GetViewByID(VIEW_ID_OMNIBOX)->GetBoundsInScreen();
  const gfx::Point tap_location = omnibox_bounds.CenterPoint();
  ASSERT_NO_FATAL_FAILURE(Tap(tap_location, tap_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Tapping in another view should clear focus and the selection.
  ASSERT_NO_FATAL_FAILURE(TapBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Tapping in the omnibox again should take focus and select all text again.
  ASSERT_NO_FATAL_FAILURE(Tap(tap_location, tap_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Tapping another omnibox spot should keep focus and selection.
  omnibox_view->SelectAll(false);
  const gfx::Point tap2_location = omnibox_bounds.origin() +
      gfx::Vector2d(omnibox_bounds.width() / 4, omnibox_bounds.height() / 4);
  ASSERT_NO_FATAL_FAILURE(Tap(tap2_location, tap2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  // We don't test if the all text is selected because it depends on whether or
  // not there was text under the tap, which appears to be flaky.

  // Take the focus away and tap in the omnibox again, but drag a bit before
  // releasing.  We should focus the omnibox but not select all of its text.
  ASSERT_NO_FATAL_FAILURE(TapBrowserWindowCenter());
  ASSERT_NO_FATAL_FAILURE(Tap(tap_location, tap2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
}
#endif // defined(USE_AURA)

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, SelectAllOnTabToFocus) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  ui_test_utils::NavigateToURL(browser(), GURL("http://www.google.com/"));
  // RevertAll after navigation to invalidate the selection range saved on blur.
  omnibox_view->RevertAll();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Pressing tab to focus the omnibox should select all text.
  while (!ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX)) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB,
                                                false, false, false, false));
  }
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());
}
