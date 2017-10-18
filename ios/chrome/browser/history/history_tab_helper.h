// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "ios/web/public/web_state/web_state_user_data.h"

namespace history {
class HistoryService;
}  // namespace history

namespace web {
class NavigationItem;
}  // namespace web

// HistoryTabHelper updates the history database based on navigation events from
// its parent WebState.
class HistoryTabHelper : public web::WebStateUserData<HistoryTabHelper> {
 public:
  ~HistoryTabHelper() override;

  // Updates history with the specified navigation.
  void UpdateHistoryForNavigation(
      const history::HistoryAddPageArgs& add_page_args);

  // Sends the page title to the history service.
  void UpdateHistoryPageTitle(const web::NavigationItem& item);

 private:
  friend class web::WebStateUserData<HistoryTabHelper>;

  // Constructs a new HistoryTabHelper.
  explicit HistoryTabHelper(web::WebState* web_state);

  // Helper function to return the history service. May return NULL, in which
  // case no history entries should be added.
  history::HistoryService* GetHistoryService();

  web::WebState* web_state_;

  DISALLOW_COPY_AND_ASSIGN(HistoryTabHelper);
};

#endif  // IOS_CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
