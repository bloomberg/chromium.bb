// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_TAB_USAGE_RECORDER_WEB_STATE_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_METRICS_TAB_USAGE_RECORDER_WEB_STATE_LIST_OBSERVER_H_

#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class TabUsageRecorder;

// Informs the TabUsageRecorder that the active Tab has changed.
class TabUsageRecorderWebStateListObserver : public WebStateListObserver {
 public:
  explicit TabUsageRecorderWebStateListObserver(
      TabUsageRecorder* tab_usage_recorder);
  ~TabUsageRecorderWebStateListObserver() override;

 private:
  // WebStateListObserver implementation.
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           bool user_action) override;

  TabUsageRecorder* tab_usage_recorder_;

  DISALLOW_COPY_AND_ASSIGN(TabUsageRecorderWebStateListObserver);
};

#endif  // IOS_CHROME_BROWSER_METRICS_TAB_USAGE_RECORDER_WEB_STATE_LIST_OBSERVER_H_
