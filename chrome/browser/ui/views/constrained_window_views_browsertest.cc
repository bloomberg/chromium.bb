// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_message.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"

#if defined(USE_AURA) && defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"
#endif

namespace {

class TestConstrainedDialogContentsView
    : public views::View,
      public base::SupportsWeakPtr<TestConstrainedDialogContentsView> {
 public:
  TestConstrainedDialogContentsView()
      : text_field_(new views::Textfield) {
    SetLayoutManager(new views::FillLayout);
    AddChildView(text_field_);
  }

  views::View* GetInitiallyFocusedView() {
    return text_field_;
  }

 private:
  views::Textfield* text_field_;
  DISALLOW_COPY_AND_ASSIGN(TestConstrainedDialogContentsView);
};

class TestConstrainedDialog : public views::DialogDelegate {
 public:
  TestConstrainedDialog()
      : contents_((new TestConstrainedDialogContentsView())->AsWeakPtr()),
        done_(false) {
  }

  virtual ~TestConstrainedDialog() {}

  virtual views::View* GetInitiallyFocusedView() OVERRIDE {
    return contents_ ? contents_->GetInitiallyFocusedView() : NULL;
  }

  virtual views::View* GetContentsView() OVERRIDE {
    return contents_.get();
  }

  virtual views::Widget* GetWidget() OVERRIDE {
    return contents_ ? contents_->GetWidget() : NULL;
  }

  virtual const views::Widget* GetWidget() const OVERRIDE {
    return contents_ ? contents_->GetWidget() : NULL;
  }

  virtual void DeleteDelegate() OVERRIDE {
    // Don't delete the delegate yet.  We need to keep it around for inspection
    // later.
    EXPECT_TRUE(done_);
  }

  virtual bool Accept() OVERRIDE {
    done_ = true;
    return true;
  }

  virtual bool Cancel() OVERRIDE {
    done_ = true;
    return true;
  }

  virtual ui::ModalType GetModalType() const OVERRIDE {
#if defined(USE_ASH)
    return ui::MODAL_TYPE_CHILD;
#else
    return views::WidgetDelegate::GetModalType();
#endif
  }

  bool done() {
    return done_;
  }

 private:
  // contents_ will be freed when the View goes away.
  base::WeakPtr<TestConstrainedDialogContentsView> contents_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(TestConstrainedDialog);
};

} // namespace

class ConstrainedWindowViewTest : public InProcessBrowserTest {
 public:
  ConstrainedWindowViewTest() {
  }
};

// Tests the following:
//
// *) Initially focused view in a constrained dialog receives focus reliably.
//
// *) Constrained windows that are queued don't register themselves as
//    accelerator targets until they are displayed.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest, FocusTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != NULL);
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  ASSERT_TRUE(web_contents_modal_dialog_manager != NULL);

  // Create a constrained dialog.  It will attach itself to web_contents.
  scoped_ptr<TestConstrainedDialog> test_dialog1(new TestConstrainedDialog);
  ConstrainedWindowViews* window1 = ConstrainedWindowViews::Create(
      web_contents, test_dialog1.get());

  views::FocusManager* focus_manager = window1->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  // test_dialog1's text field should be focused.
  EXPECT_EQ(test_dialog1->GetInitiallyFocusedView(),
            focus_manager->GetFocusedView());

  // Now create a second constrained dialog.  This will also be attached to
  // web_contents, but will remain hidden since the test_dialog1 is still
  // showing.
  scoped_ptr<TestConstrainedDialog> test_dialog2(new TestConstrainedDialog);
  ConstrainedWindowViews* window2 = ConstrainedWindowViews::Create(
      web_contents, test_dialog2.get());
  // Should be the same focus_manager.
  ASSERT_EQ(focus_manager, window2->GetFocusManager());

  // test_dialog1's text field should still be the view that has focus.
  EXPECT_EQ(test_dialog1->GetInitiallyFocusedView(),
            focus_manager->GetFocusedView());
  ASSERT_TRUE(web_contents_modal_dialog_manager->IsShowingDialog());

  // Now send a VKEY_RETURN to the browser.  This should result in closing
  // test_dialog1.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(test_dialog1->done());
  EXPECT_FALSE(test_dialog2->done());
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsShowingDialog());

  // test_dialog2 will be shown.  Focus should be on test_dialog2's text field.
  EXPECT_EQ(test_dialog2->GetInitiallyFocusedView(),
            focus_manager->GetFocusedView());

  int tab_with_constrained_window =
      browser()->tab_strip_model()->active_index();

  // Create a new tab.
  chrome::NewTab(browser());

  // The constrained dialog should no longer be selected.
  EXPECT_NE(test_dialog2->GetInitiallyFocusedView(),
            focus_manager->GetFocusedView());

  browser()->tab_strip_model()->ActivateTabAt(tab_with_constrained_window,
                                              false);

  // Activating the previous tab should bring focus to the constrained window.
  EXPECT_EQ(test_dialog2->GetInitiallyFocusedView(),
            focus_manager->GetFocusedView());

  // Send another VKEY_RETURN, closing test_dialog2
  EXPECT_TRUE(focus_manager->ProcessAccelerator(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(test_dialog2->done());
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsShowingDialog());
}

// Tests that the constrained window is closed properly when its tab is
// closed.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest, TabCloseTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != NULL);
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  ASSERT_TRUE(web_contents_modal_dialog_manager != NULL);

  // Create a constrained dialog.  It will attach itself to web_contents.
  scoped_ptr<TestConstrainedDialog> test_dialog(new TestConstrainedDialog);
  ConstrainedWindowViews::Create(
      web_contents, test_dialog.get());

  bool closed =
      browser()->tab_strip_model()->CloseWebContentsAt(
          browser()->tab_strip_model()->active_index(),
          TabStripModel::CLOSE_NONE);
  EXPECT_TRUE(closed);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(test_dialog->done());
}

// Tests that the constrained window is hidden when an other tab is selected and
// shown when its tab is selected again.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest, TabSwitchTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != NULL);

  // Create a constrained dialog.  It will attach itself to web_contents.
  scoped_ptr<TestConstrainedDialog> test_dialog(new TestConstrainedDialog);
  ConstrainedWindowViews* window = ConstrainedWindowViews::Create(
      web_contents, test_dialog.get());
  EXPECT_TRUE(window->IsVisible());

  // Open a new tab. The constrained window should hide itself.
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile())),
      true);
  EXPECT_FALSE(window->IsVisible());

  // Close the new tab. The constrained window should show itself again.
  bool closed =
      browser()->tab_strip_model()->CloseWebContentsAt(
          browser()->tab_strip_model()->active_index(),
          TabStripModel::CLOSE_NONE);
  EXPECT_TRUE(closed);
  EXPECT_TRUE(window->IsVisible());

  // Close the original tab.
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(),
      TabStripModel::CLOSE_NONE);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(test_dialog->done());
}

#if defined(OS_WIN) || (defined(USE_AURA) && defined(USE_X11))

// Forwards the key event which has |key_code| to the renderer.
void ForwardKeyEvent(content::RenderViewHost* host, ui::KeyboardCode key_code) {
#if defined(OS_WIN)
  MSG native_key_event = { NULL, WM_KEYDOWN, key_code, 0 };
#elif defined(USE_X11)
  XEvent x_event;
  ui::InitXKeyEventForTesting(
      ui::ET_KEY_PRESSED, key_code, ui::EF_NONE, &x_event);
  XEvent* native_key_event = &x_event;
#endif

#if defined(USE_AURA)
  ui::KeyEvent key(native_key_event, false);
  ui::KeyEvent* native_ui_key_event = &key;
#elif defined(OS_WIN)
  MSG native_ui_key_event = native_key_event;
#endif

  host->ForwardKeyboardEvent(
      content::NativeWebKeyboardEvent(native_ui_key_event));
}

// Tests that backspace is not processed before it's sent to the web contents.
// Flaky on win aura. crbug.com/170331
#if defined(OS_WIN) && defined(USE_AURA)
#define MAYBE_BackspaceSentToWebContent DISABLED_BackspaceSentToWebContent
#else
#define MAYBE_BackspaceSentToWebContent BackspaceSentToWebContent
#endif
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest,
                       MAYBE_BackspaceSentToWebContent) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != NULL);

  GURL new_tab_url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), new_tab_url);
  GURL about_url(chrome::kChromeUIAboutURL);
  ui_test_utils::NavigateToURL(browser(), about_url);

  ConstrainedWebDialogDelegate* cwdd = CreateConstrainedWebDialog(
      browser()->profile(),
      new ui::test::TestWebDialogDelegate(about_url),
      NULL,
      web_contents);

  content::RenderViewHost* render_view_host =
      cwdd->GetWebContents()->GetRenderViewHost();
  ForwardKeyEvent(render_view_host, ui::VKEY_BACK);

  // Backspace is not processed as accelerator before it's sent to web contents.
  EXPECT_EQ(about_url.spec(), web_contents->GetURL().spec());

  content::RunAllPendingInMessageLoop();

  // Backspace is processed as accelerator after it's sent to web contents.
  EXPECT_EQ(new_tab_url.spec(), web_contents->GetURL().spec());
}

// Fails flakily (once per 10-20 runs) on Win Aura only. http://crbug.com/177482
#if defined(OS_WIN)
#define MAYBE_EscapeCloseConstrainedWindow DISABLED_EscapeCloseConstrainedWindow
#else
#define MAYBE_EscapeCloseConstrainedWindow EscapeCloseConstrainedWindow
#endif

// Tests that escape closes the constrained window.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest,
                       MAYBE_EscapeCloseConstrainedWindow) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != NULL);

  GURL new_tab_url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), new_tab_url);
  ConstrainedWebDialogDelegate* cwdd = CreateConstrainedWebDialog(
      browser()->profile(),
      new ui::test::TestWebDialogDelegate(new_tab_url),
      NULL,
      web_contents);

  ConstrainedWindowViews* cwv =
      static_cast<ConstrainedWindowViews*>(cwdd->GetWindow());
  views::test::TestWidgetObserver observer(cwv);
  cwv->FocusWebContentsModalDialog();

  content::RenderViewHost* render_view_host =
      cwdd->GetWebContents()->GetRenderViewHost();
  ForwardKeyEvent(render_view_host, ui::VKEY_ESCAPE);

  // Escape is not processed as accelerator before it's sent to web contents.
  EXPECT_FALSE(observer.widget_closed());

  content::RunAllPendingInMessageLoop();

  // Escape is processed as accelerator after it's sent to web contents.
  EXPECT_TRUE(observer.widget_closed());
}

#endif  // defined(OS_WIN) || (defined(USE_AURA) && defined(USE_X11))
