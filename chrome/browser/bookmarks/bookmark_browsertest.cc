// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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
    if (!bookmark_model->IsLoaded()) {
      content::WindowedNotificationObserver observer(
          chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
          content::NotificationService::AllSources());
      observer.Wait();
    }
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

  bookmark_utils::AddIfNotBookmarked(
      bookmark_model, GURL(kPersistBookmarkURL),
      ASCIIToUTF16(kPersistBookmarkTitle));
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, Persist) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());

  std::vector<BookmarkService::URLAndTitle> urls;
  bookmark_model->GetBookmarks(&urls);

  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(GURL(kPersistBookmarkURL), urls[0].url);
  ASSERT_EQ(ASCIIToUTF16(kPersistBookmarkTitle), urls[0].title);
}

#if !defined(OS_CHROMEOS)  // No multi-profile on ChromeOS.

// Sanity check that bookmarks from different profiles are separate.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, MultiProfile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  BookmarkModel* bookmark_model1 = WaitForBookmarkModel(browser()->profile());

  ui_test_utils::BrowserAddedObserver observer;
  g_browser_process->profile_manager()->CreateMultiProfileAsync(
      string16(), string16(), ProfileManager::CreateCallback());
  Browser* browser2 = observer.WaitForSingleNewBrowser();
  BookmarkModel* bookmark_model2 = WaitForBookmarkModel(browser2->profile());

  bookmark_utils::AddIfNotBookmarked(
      bookmark_model1, GURL(kPersistBookmarkURL),
      ASCIIToUTF16(kPersistBookmarkTitle));
  std::vector<BookmarkService::URLAndTitle> urls1, urls2;
  bookmark_model1->GetBookmarks(&urls1);
  bookmark_model2->GetBookmarks(&urls2);
  ASSERT_EQ(1u, urls1.size());
  ASSERT_TRUE(urls2.empty());
}

#endif
