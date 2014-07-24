// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include "base/command_line.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_testing_views.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/text_input_focus_manager.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

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

  // Tap the center of the browser window.
  void TapBrowserWindowCenter() {
    gfx::Point center = BrowserView::GetBrowserViewForBrowser(
        browser())->GetBoundsInScreen().CenterPoint();
    ui::test::EventGenerator generator(
        browser()->window()->GetNativeWindow()->GetRootWindow());
    generator.GestureTapAt(center);
  }

  // Touch down and release at the specified locations.
  void Tap(const gfx::Point& press_location,
           const gfx::Point& release_location) {
    ui::EventProcessor* dispatcher =
        browser()->window()->GetNativeWindow()->GetHost()->event_processor();

    base::TimeDelta timestamp = ui::EventTimeForNow();
    ui::TouchEvent press(
        ui::ET_TOUCH_PRESSED, press_location, 5, timestamp);
    ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&press);
    ASSERT_FALSE(details.dispatcher_destroyed);

    if (press_location != release_location) {
      timestamp += base::TimeDelta::FromMilliseconds(10);
      ui::TouchEvent move(
          ui::ET_TOUCH_MOVED, release_location, 5, timestamp);
      details = dispatcher->OnEventFromSource(&move);
    }

    timestamp += base::TimeDelta::FromMilliseconds(50);
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED, release_location, 5, timestamp);
    details = dispatcher->OnEventFromSource(&release);
    ASSERT_FALSE(details.dispatcher_destroyed);
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
  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
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

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, CloseOmniboxPopupOnTextDrag) {
  OmniboxView* omnibox_view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->model()->popup_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  AutocompleteMatch match;
  match.destination_url = GURL("http://autocomplete-result/");
  match.allowed_to_be_default_match = true;
  match.type = AutocompleteMatchType::HISTORY_TITLE;
  match.relevance = 500;
  matches.push_back(match);
  match.destination_url = GURL("http://autocomplete-result2/");
  matches.push_back(match);
  results.AppendMatches(matches);
  results.SortAndCull(
      AutocompleteInput(),
      TemplateURLServiceFactory::GetForProfile(browser()->profile()));

  // The omnibox popup should open with suggestions displayed.
  omnibox_view->model()->popup_model()->OnResultChanged();
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());

  // The omnibox text should be selected.
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Simulate a mouse click before dragging the mouse.
  gfx::Point point(omnibox_view_views->x(), omnibox_view_views->y());
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  omnibox_view_views->OnMousePressed(pressed);
  EXPECT_TRUE(omnibox_view->model()->popup_model()->IsOpen());

  // Simulate a mouse drag of the omnibox text, and the omnibox should close.
  ui::MouseEvent dragged(ui::ET_MOUSE_DRAGGED, point, point,
                         ui::EF_LEFT_MOUSE_BUTTON, 0);
  omnibox_view_views->OnMouseDragged(dragged);

  EXPECT_FALSE(omnibox_view->model()->popup_model()->IsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, BackgroundIsOpaque) {
  // The omnibox text should be rendered on an opaque background. Otherwise, we
  // can't use subpixel rendering.
  BrowserWindowTesting* window = browser()->window()->GetBrowserWindowTesting();
  ASSERT_TRUE(window);
  OmniboxViewViews* view = window->GetLocationBarView()->omnibox_view();
  ASSERT_TRUE(view);
  EXPECT_FALSE(view->GetRenderText()->background_is_transparent());
}

// Tests if executing a command hides touch editing handles.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest,
                       DeactivateTouchEditingOnExecuteCommand) {
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableTouchEditing);

  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);
  views::TextfieldTestApi textfield_test_api(omnibox_view_views);

  // Put a URL on the clipboard. It is written to the clipboard upon destruction
  // of the writer.
  {
    ui::ScopedClipboardWriter clipboard_writer(
        ui::Clipboard::GetForCurrentThread(),
        ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteURL(base::ASCIIToUTF16("http://www.example.com/"));
  }

  // Tap to activate touch editing.
  gfx::Point omnibox_center =
      omnibox_view_views->GetBoundsInScreen().CenterPoint();
  Tap(omnibox_center, omnibox_center);
  EXPECT_TRUE(textfield_test_api.touch_selection_controller());

  // Execute a command and check if it deactivate touch editing. Paste & Go is
  // chosen since it is specific to Omnibox and its execution wouldn't be
  // delgated to the base Textfield class.
  omnibox_view_views->ExecuteCommand(IDS_PASTE_AND_GO, ui::EF_NONE);
  EXPECT_FALSE(textfield_test_api.touch_selection_controller());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, FocusedTextInputClient) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(switches::kEnableTextInputFocusManager);

  // TODO(yukishiino): The following call to FocusLocationBar is not necessary
  // if the flag is enabled by default.  Remove the call once the transition to
  // TextInputFocusManager completes.
  chrome::FocusLocationBar(browser());
  OmniboxView* view = NULL;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);
  ui::TextInputFocusManager* text_input_focus_manager =
      ui::TextInputFocusManager::GetInstance();
  EXPECT_EQ(omnibox_view_views->GetTextInputClient(),
            text_input_focus_manager->GetFocusedTextInputClient());
}
