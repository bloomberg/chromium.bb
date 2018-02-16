// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_BROWSER_STATE_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_SIGNIN_BROWSER_STATE_DATA_REMOVER_H_

#include <memory>

#include "base/ios/block_types.h"
#include "base/macros.h"

namespace ios {
class ChromeBrowserState;
}

namespace reading_list {
class ReadingListRemoverHelper;
}

// Helper that wipes all the data in the given browser state. This deletes all
// browsing data and all the bookmarks.
class BrowserStateDataRemover {
 public:
  explicit BrowserStateDataRemover(ios::ChromeBrowserState* browser_state);
  ~BrowserStateDataRemover();

  // Removes all bookmarks, clears all browsing data, last signed-in username
  // and then runs |completion|. The user must be signed out when this method
  // is called.
  static void ClearData(ios::ChromeBrowserState* browser_state,
                        ProceduralBlock completion);

 private:
  // Wipes all the data in the browser state and invokes |callback| when done.
  // This can be called only once, and this object deletes itself after invoking
  // the callback.
  void RemoveBrowserStateData(ProceduralBlock callback);

  void BrowsingDataCleared();
  void ReadingListCleaned(bool reading_list_cleaned);

  ios::ChromeBrowserState* browser_state_;
  ProceduralBlock callback_;
  std::unique_ptr<reading_list::ReadingListRemoverHelper>
      reading_list_remover_helper_;

  DISALLOW_COPY_AND_ASSIGN(BrowserStateDataRemover);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_BROWSER_STATE_DATA_REMOVER_H_
