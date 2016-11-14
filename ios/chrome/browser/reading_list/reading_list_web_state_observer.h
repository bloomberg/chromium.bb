// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_WEB_STATE_OBSERVER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ios/web/public/web_state/web_state_observer.h"

class BrowserState;
class ReadingListModel;

// Observes the loading of pages coming from the reading list, determines
// whether loading an offline version of the page is needed, and actually
// trigger the loading of the offline page (if possible).
class ReadingListWebStateObserver : public web::WebStateObserver {
 public:
  static ReadingListWebStateObserver* FromWebState(
      web::WebState* web_state,
      ReadingListModel* reading_list_model);

  ~ReadingListWebStateObserver() override;

  // Starts checking that the current navigation is loading quickly enough [1].
  // If not, starts to load a distilled version of the page (if there is any).
  // If that same WebStateObserver was already checking that a page was loading
  // quickly enough, stops checking the loading of that page.
  // [1] A page loading quickly enough is a page that has loaded 15% within
  // 1 second.
  void StartCheckingProgress();

 private:
  ReadingListWebStateObserver(web::WebState* web_state,
                              ReadingListModel* reading_list_model);

  // Looks at the loading percentage. If less than 15%, attemps to load the
  // offline version of that page.
  void VerifyIfReadingListEntryStartedLoading();

  friend class ReadingListWebStateObserverUserDataWrapper;

  // WebContentsObserver implementation.
  void DidStopLoading() override;
  void PageLoaded(
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed() override;

  ReadingListModel* reading_list_model_;
  std::unique_ptr<base::Timer> timer_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListWebStateObserver);
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_WEB_STATE_OBSERVER_H_
