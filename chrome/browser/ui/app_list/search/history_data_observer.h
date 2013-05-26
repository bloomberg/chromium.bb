// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_DATA_OBSERVER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_DATA_OBSERVER_H_

namespace app_list {

class HistoryDataObserver {
 public:
  // Invoked when the data is loaded from underlying store.
  virtual void OnHistoryDataLoadedFromStore() = 0;

 protected:
  virtual ~HistoryDataObserver() {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_DATA_OBSERVER_H_
