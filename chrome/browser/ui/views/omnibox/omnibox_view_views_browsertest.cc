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
#include "chrome/browser/ui/views/omnibox/omnibox_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/textfield/native_textfield_wrapper.h"

class OmniboxViewViewsTest : public InProcessBrowserTest {
 protected:
  OmniboxViewViewsTest() {}

  static void GetOmniboxViewForBrowser(const Browser* browser,
                                       OmniboxView** omnibox_view) {
    BrowserWindow* window = browser->window();
    ASSERT_TRUE(window);
    LocationBar* location_bar = window->GetLocationBar();
    ASSERT_TRUE(location_bar);
    *omnibox_view = location_bar->GetLocationEntry();
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

  // Press and release the mouse in the omnibox at an offset from its origin.
  // If |release_offset| differs from |press_offset|, the mouse will be moved
  // between the press and release.
  void ClickOmnibox(ui_controls::MouseButton button,
                    const gfx::Vector2d& press_offset,
                    const gfx::Vector2d& release_offset) {
    const views::View* omnibox = BrowserView::GetBrowserViewForBrowser(
        browser())->GetViewByID(VIEW_ID_OMNIBOX);
    gfx::Point omnibox_origin = omnibox->GetBoundsInScreen().origin();
    gfx::Point press_point = omnibox_origin + press_offset;
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(press_point));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(button, ui_controls::DOWN));

    gfx::Point release_point = omnibox_origin + release_offset;
    if (release_point != press_point)
      ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(release_point));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(button, ui_controls::UP));
  }

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
  OmniboxView* view = browser()->window()->GetLocationBar()->GetLocationEntry();
  OmniboxViewViews* omnibox_view_views = GetOmniboxViewViews(view);
  // This test is only relevant when OmniboxViewViews is present and is using
  // the native textfield wrapper.
  if (!omnibox_view_views)
    return;
  views::NativeTextfieldWrapper* native_textfield_wrapper =
      static_cast<views::NativeTextfieldWrapper*>(
          omnibox_view_views->GetNativeWrapperForTesting());
  if (!native_textfield_wrapper)
    return;

  // Put an URL on the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(
        ui::Clipboard::GetForCurrentThread(), ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteURL(ASCIIToUTF16("http://www.example.com/"));
  }

  // Paste and go.
  native_textfield_wrapper->ExecuteTextCommand(IDS_PASTE_AND_GO);

  // The popup should not be open.
  EXPECT_FALSE(view->model()->popup_model()->IsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, SelectAllOnClick) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  omnibox_view->SetUserText(ASCIIToUTF16("http://www.google.com/"));
  const gfx::Vector2d click(40, 10);

  // Take the focus away from the omnibox.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Clicking in the omnibox should take focus and select all text.
  ASSERT_NO_FATAL_FAILURE(ClickOmnibox(ui_controls::LEFT, click, click));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Clicking in another view should clear focus and the selection.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Clicking in the omnibox again should take focus and select all text again.
  ASSERT_NO_FATAL_FAILURE(ClickOmnibox(ui_controls::LEFT, click, click));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Clicking another omnibox spot should keep focus but clear the selection.
  omnibox_view->SelectAll(false);
  const gfx::Vector2d click_2(click.x() + 10, click.y());
  ASSERT_NO_FATAL_FAILURE(ClickOmnibox(ui_controls::LEFT, click_2, click_2));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Take the focus away and click in the omnibox again, but drag a bit before
  // releasing.  We should focus the omnibox but not select all of its text.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  const gfx::Vector2d release(click.x() + 10, click.y());
  ASSERT_NO_FATAL_FAILURE(ClickOmnibox(ui_controls::LEFT, click, release));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Middle-clicking should not be handled by the omnibox.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  ASSERT_NO_FATAL_FAILURE(ClickOmnibox(ui_controls::MIDDLE, click, click));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
}
