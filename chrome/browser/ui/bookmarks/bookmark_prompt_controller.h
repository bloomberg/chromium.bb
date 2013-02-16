// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMPT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class Browser;
class PrefService;

namespace content {
class WebContents;
}

// BookmarkPromptController is a kind of singleton object held by
// BrowserProcessImpl, and controls showing bookmark prompt for frequently
// visited URLs.
class BookmarkPromptController : public chrome::BrowserListObserver,
                                 public content::NotificationObserver,
                                 public TabStripModelObserver {
 public:
  // When impression count is greater than |kMaxPromptImpressionCount|, we
  // don't display bookmark prompt anymore.
  static const int kMaxPromptImpressionCount;

  // When user visited the URL 10 times, we show the bookmark prompt.
  static const int kVisitCountForPermanentTrigger;

  // When user visited the URL 3 times last 24 hours, we show the bookmark
  // prompt.
  static const int kVisitCountForSessionTrigger;

  // An instance of BookmarkPromptController is constructed only if bookmark
  // feature enabled.
  BookmarkPromptController();
  virtual ~BookmarkPromptController();

  // Invoked when bookmark is added for |url| by clicking star icon in
  // |browser|. Note: Clicking bookmark menu item in action box is also
  // considered as clicking star icon.
  static void AddedBookmark(Browser* browser, const GURL& url);

  // Invoked when bookmark prompt is closing.
  static void ClosingBookmarkPrompt();

  // Disable bookmark prompt feature in a profile in |prefs|.
  static void DisableBookmarkPrompt(PrefService* prefs);

  // True if bookmark prompt feature is enabled, otherwise false.
  static bool IsEnabled();

 private:
  // TabStripModelObserver
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;

  void AddedBookmarkInternal(Browser* browser, const GURL& url);
  void ClosingBookmarkPromptInternal();

   // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // chrome::BrowserListObserver
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;
  virtual void OnBrowserSetLastActive(Browser* browser) OVERRIDE;

  // Callback for the HistoryService::QueryURL
  void OnDidQueryURL(CancelableRequestProvider::Handle handle,
                     bool success,
                     const history::URLRow* url_row,
                     history::VisitVector* visits);

  // Set current active browser cache to |browser|. |browser| can be null.
  void SetBrowser(Browser* browser);

  // Set current active WebContents cache from |web_contents|. |web_contents|
  // can be null.
  void SetWebContents(content::WebContents* web_contents);

  // Current active browser cache which we will display the prompt on it.
  Browser* browser_;

  // Current active WebContents cache which we will display the prompt for it.
  content::WebContents* web_contents_;

  // Remember last URL for recording metrics.
  GURL last_prompted_url_;

  // Last prompted time is used for measuring duration of prompt displaying
  // time.
  base::Time last_prompted_time_;

  // Start time of HistoryService::QueryURL.
  base::Time query_url_start_time_;

  CancelableRequestConsumer query_url_consumer_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkPromptController);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PROMPT_CONTROLLER_H_
