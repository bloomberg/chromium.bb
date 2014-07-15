// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"

namespace {
const char kPersistBookmarkURL[] = "http://www.cnn.com/";
const char kPersistBookmarkTitle[] = "CNN";
}

class BookmarkBrowsertest : public InProcessBrowserTest {
 public:
  BookmarkBrowsertest() {}

  bool IsVisible() {
    return browser()->bookmark_bar_state() == BookmarkBar::SHOW;
  }

  static void CheckAnimation(Browser* browser, const base::Closure& quit_task) {
    if (!browser->window()->IsBookmarkBarAnimating())
      quit_task.Run();
  }

  base::TimeDelta WaitForBookmarkBarAnimationToFinish() {
    base::Time start(base::Time::Now());
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;

    base::Timer timer(false, true);
    timer.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(15),
        base::Bind(&CheckAnimation, browser(), runner->QuitClosure()));
    runner->Run();
    return base::Time::Now() - start;
  }

  BookmarkModel* WaitForBookmarkModel(Profile* profile) {
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForProfile(profile);
    test::WaitForBookmarkModelToLoad(bookmark_model);
    return bookmark_model;
  }
};

// Test of bookmark bar toggling, visibility, and animation.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, BookmarkBarVisibleWait) {
  ASSERT_FALSE(IsVisible());
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
  base::TimeDelta delay = WaitForBookmarkBarAnimationToFinish();
  LOG(INFO) << "Took " << delay.InMilliseconds() << " ms to show bookmark bar";
  ASSERT_TRUE(IsVisible());
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
  delay = WaitForBookmarkBarAnimationToFinish();
  LOG(INFO) << "Took " << delay.InMilliseconds() << " ms to hide bookmark bar";
  ASSERT_FALSE(IsVisible());
}

// Verify that bookmarks persist browser restart.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, PRE_Persist) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());

  bookmarks::AddIfNotBookmarked(bookmark_model,
                                GURL(kPersistBookmarkURL),
                                base::ASCIIToUTF16(kPersistBookmarkTitle));
}

#if defined(THREAD_SANITIZER)
// BookmarkBrowsertest.Persist fails under ThreadSanitizer on Linux, see
// http://crbug.com/340223.
#define MAYBE_Persist DISABLED_Persist
#else
#define MAYBE_Persist Persist
#endif
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, MAYBE_Persist) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());

  std::vector<BookmarkModel::URLAndTitle> urls;
  bookmark_model->GetBookmarks(&urls);

  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(GURL(kPersistBookmarkURL), urls[0].url);
  ASSERT_EQ(base::ASCIIToUTF16(kPersistBookmarkTitle), urls[0].title);
}

#if !defined(OS_CHROMEOS)  // No multi-profile on ChromeOS.

// Sanity check that bookmarks from different profiles are separate.
// DISABLED_ because it regularly times out: http://crbug.com/159002.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, DISABLED_MultiProfile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  BookmarkModel* bookmark_model1 = WaitForBookmarkModel(browser()->profile());

  ui_test_utils::BrowserAddedObserver observer;
  g_browser_process->profile_manager()->CreateMultiProfileAsync(
      base::string16(), base::string16(), ProfileManager::CreateCallback(),
      std::string());
  Browser* browser2 = observer.WaitForSingleNewBrowser();
  BookmarkModel* bookmark_model2 = WaitForBookmarkModel(browser2->profile());

  bookmarks::AddIfNotBookmarked(bookmark_model1,
                                GURL(kPersistBookmarkURL),
                                base::ASCIIToUTF16(kPersistBookmarkTitle));
  std::vector<BookmarkModel::URLAndTitle> urls1, urls2;
  bookmark_model1->GetBookmarks(&urls1);
  bookmark_model2->GetBookmarks(&urls2);
  ASSERT_EQ(1u, urls1.size());
  ASSERT_TRUE(urls2.empty());
}

#endif
