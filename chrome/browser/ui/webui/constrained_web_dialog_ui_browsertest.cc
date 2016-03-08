// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"

using content::WebContents;
using ui::WebDialogDelegate;
using web_modal::WebContentsModalDialogManager;

namespace {

#if !defined(OS_MACOSX)
static const char kTestDataURL[] = "data:text/html,<!doctype html>"
    "<body></body>"
    "<style>"
    "body { height: 150px; width: 150px; }"
    "</style>";

bool IsEqualSizes(gfx::Size expected,
                  ConstrainedWebDialogDelegate* dialog_delegate) {
  return expected == dialog_delegate->GetPreferredSize();
}

std::string GetChangeDimensionsScript(int dimension) {
  return base::StringPrintf("window.document.body.style.width = %d + 'px';"
      "window.document.body.style.height = %d + 'px';", dimension, dimension);
}
#endif

class ConstrainedWebDialogBrowserTestObserver
    : public content::WebContentsObserver {
 public:
  explicit ConstrainedWebDialogBrowserTestObserver(WebContents* contents)
      : content::WebContentsObserver(contents),
        contents_destroyed_(false) {
  }
  ~ConstrainedWebDialogBrowserTestObserver() override {}

  bool contents_destroyed() { return contents_destroyed_; }

 private:
  void WebContentsDestroyed() override { contents_destroyed_ = true; }

  bool contents_destroyed_;
};

class AutoResizingTestWebDialogDelegate
    : public ui::test::TestWebDialogDelegate {
 public:
  explicit AutoResizingTestWebDialogDelegate(const GURL& url)
      : TestWebDialogDelegate(url) {}
  ~AutoResizingTestWebDialogDelegate() override {}

  // Dialog delegates for auto-resizing dialogs are expected not to set |size|.
  void GetDialogSize(gfx::Size* size) const override {}
};

}  // namespace

class ConstrainedWebDialogBrowserTest : public InProcessBrowserTest {
 public:
  ConstrainedWebDialogBrowserTest() {}

  // Runs the current MessageLoop until |condition| is true or timeout.
  bool RunLoopUntil(const base::Callback<bool()>& condition) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    while (!condition.Run()) {
      const base::TimeTicks current_time = base::TimeTicks::Now();
      if (current_time - start_time > base::TimeDelta::FromSeconds(5)) {
        ADD_FAILURE() << "Condition not met within five seconds.";
        return false;
      }

      base::MessageLoop::current()->task_runner()->PostDelayedTask(
          FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
          base::TimeDelta::FromMilliseconds(20));
      content::RunMessageLoop();
    }
    return true;
  }

 protected:
  bool IsShowingWebContentsModalDialog(WebContents* web_contents) const {
    WebContentsModalDialogManager* web_contents_modal_dialog_manager =
        WebContentsModalDialogManager::FromWebContents(web_contents);
    return web_contents_modal_dialog_manager->IsDialogActive();
  }
};

// Tests that opening/closing the constrained window won't crash it.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest, BasicTest) {
  // The delegate deletes itself.
  WebDialogDelegate* delegate = new ui::test::TestWebDialogDelegate(
      GURL(chrome::kChromeUIConstrainedHTMLTestURL));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ConstrainedWebDialogDelegate* dialog_delegate =
      ShowConstrainedWebDialog(browser()->profile(), delegate, web_contents);
  ASSERT_TRUE(dialog_delegate);
  EXPECT_TRUE(dialog_delegate->GetNativeDialog());
  EXPECT_TRUE(IsShowingWebContentsModalDialog(web_contents));
}

// Tests that ReleaseWebContentsOnDialogClose() works.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest,
                       ReleaseWebContentsOnDialogClose) {
  // The delegate deletes itself.
  WebDialogDelegate* delegate = new ui::test::TestWebDialogDelegate(
      GURL(chrome::kChromeUIConstrainedHTMLTestURL));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ConstrainedWebDialogDelegate* dialog_delegate =
      ShowConstrainedWebDialog(browser()->profile(), delegate, web_contents);
  ASSERT_TRUE(dialog_delegate);
  scoped_ptr<WebContents> new_tab(dialog_delegate->GetWebContents());
  ASSERT_TRUE(new_tab.get());
  ASSERT_TRUE(IsShowingWebContentsModalDialog(web_contents));

  ConstrainedWebDialogBrowserTestObserver observer(new_tab.get());
  dialog_delegate->ReleaseWebContentsOnDialogClose();
  dialog_delegate->OnDialogCloseFromWebUI();

  ASSERT_FALSE(observer.contents_destroyed());
  EXPECT_FALSE(IsShowingWebContentsModalDialog(web_contents));
  new_tab.reset();
  EXPECT_TRUE(observer.contents_destroyed());
}

#if !defined(OS_MACOSX)
// Tests that dialog autoresizes based on web contents when autoresizing
// is enabled.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest,
                       ContentResizeInAutoResizingDialog) {
  // During auto-resizing, dialogs size to (WebContents size) + 16.
  const int dialog_border_space = 16;

  // Expected dialog sizes after auto-resizing.
  const int initial_size = 150 + dialog_border_space;
  const int new_size = 175 + dialog_border_space;

  // The delegate deletes itself.
  WebDialogDelegate* delegate =
      new AutoResizingTestWebDialogDelegate(GURL(kTestDataURL));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Observes the next created WebContents.
  content::TestNavigationObserver observer(NULL);
  observer.StartWatchingNewWebContents();

  gfx::Size min_size = gfx::Size(100, 100);
  gfx::Size max_size = gfx::Size(200, 200);
  gfx::Size initial_dialog_size;
  delegate->GetDialogSize(&initial_dialog_size);

  ConstrainedWebDialogDelegate* dialog_delegate =
      ShowConstrainedWebDialogWithAutoResize(browser()->profile(), delegate,
                                               web_contents, min_size,
                                               max_size);
  ASSERT_TRUE(dialog_delegate);
  EXPECT_TRUE(dialog_delegate->GetNativeDialog());
  ASSERT_FALSE(IsShowingWebContentsModalDialog(web_contents));
  EXPECT_EQ(min_size, dialog_delegate->GetMinimumSize());
  EXPECT_EQ(max_size, dialog_delegate->GetMaximumSize());

  // Check for initial sizing. Dialog was created as a 400x400 dialog.
  EXPECT_EQ(gfx::Size(), web_contents->GetPreferredSize());
  ASSERT_EQ(initial_dialog_size, dialog_delegate->GetPreferredSize());

  observer.Wait();

  // Wait until the entire WebContents has loaded.
  WaitForLoadStop(dialog_delegate->GetWebContents());

  ASSERT_TRUE(IsShowingWebContentsModalDialog(web_contents));

  // Resize to content's originally set dimensions.
  ASSERT_TRUE(RunLoopUntil(base::Bind(
      &IsEqualSizes,
      gfx::Size(initial_size, initial_size),
      dialog_delegate)));

  // Resize to dimensions within expected bounds.
  EXPECT_TRUE(ExecuteScript(dialog_delegate->GetWebContents(),
      GetChangeDimensionsScript(175)));
  ASSERT_TRUE(RunLoopUntil(base::Bind(
      &IsEqualSizes,
      gfx::Size(new_size, new_size),
      dialog_delegate)));

  // Resize to dimensions smaller than the minimum bounds.
  EXPECT_TRUE(ExecuteScript(dialog_delegate->GetWebContents(),
      GetChangeDimensionsScript(50)));
  ASSERT_TRUE(RunLoopUntil(base::Bind(
      &IsEqualSizes,
      min_size,
      dialog_delegate)));

  // Resize to dimensions greater than the maximum bounds.
  EXPECT_TRUE(ExecuteScript(dialog_delegate->GetWebContents(),
      GetChangeDimensionsScript(250)));
  ASSERT_TRUE(RunLoopUntil(base::Bind(
      &IsEqualSizes,
      max_size,
      dialog_delegate)));
}

// Tests that dialog does not autoresize when autoresizing is not enabled.
IN_PROC_BROWSER_TEST_F(ConstrainedWebDialogBrowserTest,
                       ContentResizeInNonAutoResizingDialog) {
  // The delegate deletes itself.
  WebDialogDelegate* delegate =
      new ui::test::TestWebDialogDelegate(GURL(kTestDataURL));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ConstrainedWebDialogDelegate* dialog_delegate =
      ShowConstrainedWebDialog(browser()->profile(), delegate, web_contents);
  ASSERT_TRUE(dialog_delegate);
  EXPECT_TRUE(dialog_delegate->GetNativeDialog());
  EXPECT_TRUE(IsShowingWebContentsModalDialog(web_contents));

  // Wait until the entire WebContents has loaded.
  WaitForLoadStop(dialog_delegate->GetWebContents());

  gfx::Size initial_dialog_size;
  delegate->GetDialogSize(&initial_dialog_size);

  // Check for initial sizing. Dialog was created as a 400x400 dialog.
  EXPECT_EQ(gfx::Size(), web_contents->GetPreferredSize());
  ASSERT_EQ(initial_dialog_size, dialog_delegate->GetPreferredSize());

  // Resize <body> to dimension smaller than dialog.
  EXPECT_TRUE(ExecuteScript(dialog_delegate->GetWebContents(),
      GetChangeDimensionsScript(100)));
  ASSERT_TRUE(RunLoopUntil(base::Bind(
      &IsEqualSizes,
      initial_dialog_size,
      dialog_delegate)));

  // Resize <body> to dimension larger than dialog.
  EXPECT_TRUE(ExecuteScript(dialog_delegate->GetWebContents(),
      GetChangeDimensionsScript(500)));
  ASSERT_TRUE(RunLoopUntil(base::Bind(
      &IsEqualSizes,
      initial_dialog_size,
      dialog_delegate)));
}
#endif  // !OS_MACOSX
