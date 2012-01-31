// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/web_contents.h"

class MediaBrowserTest : public InProcessBrowserTest {
 public:
  MediaBrowserTest()
      : seek_jumper_url_(ui_test_utils::GetTestUrl(
          FilePath(FILE_PATH_LITERAL("media")),
          FilePath(FILE_PATH_LITERAL("seek-jumper.html")))) {
  }

 protected:
  const GURL seek_jumper_url_;
};

class TabWatcher : public TabStripModelObserver {
 public:
  explicit TabWatcher(Browser* browser)
      : browser_(browser), in_run_(false),
        expected_title_tab_(-1), expected_tab_count_(-1) {
    browser->tabstrip_model()->AddObserver(this);
  }

  void WaitForTitleToBe(int tab_index,
                        const base::StringPiece& expected_title) {
    DCHECK(expected_title_tab_ == -1 && expected_tab_count_ == -1);
    expected_title_tab_ = tab_index;
    expected_title_ = expected_title;
    Run();
  }

  void WaitForTabCountToBe(int expected_tab_count) {
    DCHECK(expected_title_tab_ == -1 && expected_tab_count_ == -1);
    expected_tab_count_ = expected_tab_count;
    Run();
  }

  virtual ~TabWatcher() {
    if (browser_)
      browser_->tabstrip_model()->RemoveObserver(this);
  }

 private:

  void Run() {
    CHECK(!in_run_);
    in_run_ = true;
    ui_test_utils::RunMessageLoop();
    in_run_ = false;
  }

  void QuitIfExpectationReached() {
    bool quit = false;
    CHECK(browser_);
    CHECK(in_run_);
    if ((expected_tab_count_ >= 0) &&
        (browser_->tabstrip_model()->count() == expected_tab_count_)) {
      quit = true;
    } else if ((expected_title_tab_ >= 0) && EqualsASCII(
        browser_->GetWebContentsAt(expected_title_tab_)->GetTitle(),
        expected_title_)) {
      quit = true;
    }
    if (quit) {
      expected_title_tab_ = -1;
      expected_tab_count_ = -1;
      MessageLoopForUI::current()->Quit();
    }
  }

  virtual void TabChangedAt(TabContentsWrapper*, int, TabChangeType) OVERRIDE {
    QuitIfExpectationReached();
  }
  virtual void TabInsertedAt(TabContentsWrapper*, int, bool) OVERRIDE {
    QuitIfExpectationReached();
  }
  virtual void TabClosingAt(TabStripModel*, TabContentsWrapper*, int) OVERRIDE {
    QuitIfExpectationReached();
  }
  virtual void TabStripEmpty() OVERRIDE {
    browser_ = NULL;
  }
  virtual void TabStripModelDeleted() OVERRIDE {
    browser_ = NULL;
  }

  Browser* browser_;
  bool in_run_;
  int expected_title_tab_;  // -1 if not waiting for a title.
  base::StringPiece expected_title_;  // Ignored if not waiting for a title.
  int expected_tab_count_;  // -1 if not waiting for a tab count.

};

#if defined(OS_MACOSX)
// Mac still has bugs when it comes to lots of outstanding seeks
// http://crbug.com/102395
#define MAYBE_SeekJumper_Alone DISABLED_SeekJumper_Alone
#else
#define MAYBE_SeekJumper_Alone SeekJumper_Alone
#endif

// Regression test: pending seeks shouldn't crash the browser when the tab is
// closed.
IN_PROC_BROWSER_TEST_F(MediaBrowserTest, MAYBE_SeekJumper_Alone) {
  TabWatcher watcher(browser());
  ui_test_utils::NavigateToURL(browser(), seek_jumper_url_);
  watcher.WaitForTabCountToBe(1);
  watcher.WaitForTitleToBe(0, "Done");
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_W, true, false, false, false));
  watcher.WaitForTabCountToBe(0);
  // Lack of crash is our SUCCESS.
}

#if defined(OS_MACOSX)
// Mac still has bugs when it comes to lots of outstanding seeks
// http://crbug.com/102395
#define MAYBE_SeekJumper_SharedRenderer DISABLED_SeekJumper_SharedRenderer
#else
#define MAYBE_SeekJumper_SharedRenderer SeekJumper_SharedRenderer
#endif

// Regression test: pending seeks shouldn't crash a shared renderer when the tab
// containing the audio element is closed.
IN_PROC_BROWSER_TEST_F(MediaBrowserTest, MAYBE_SeekJumper_SharedRenderer) {
  TabWatcher watcher(browser());
  ui_test_utils::NavigateToURL(browser(), seek_jumper_url_);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), seek_jumper_url_, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  watcher.WaitForTabCountToBe(2);
  watcher.WaitForTitleToBe(0, "Done");
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_W, true, false, false, false));
  watcher.WaitForTabCountToBe(1);
  watcher.WaitForTitleToBe(0, "Done");
  // Give the renderer a bit of time to crash.  Sad but necessary.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, MessageLoop::QuitClosure(), TestTimeouts::action_timeout());
  ui_test_utils::RunMessageLoop();
  ASSERT_TRUE(browser()->GetWebContentsAt(0)->GetRenderViewHost()->
              IsRenderViewLive());
}
