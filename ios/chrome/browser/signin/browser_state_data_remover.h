// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_BROWSER_STATE_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_SIGNIN_BROWSER_STATE_DATA_REMOVER_H_

#include "base/ios/block_types.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/browsing_data/ios_chrome_browsing_data_remover.h"

namespace ios {
class ChromeBrowserState;
}

// Helper that wipes all the data in the given browser state. This deletes all
// browsing data and all the bookmarks.
class BrowserStateDataRemover {
 public:
  explicit BrowserStateDataRemover(ios::ChromeBrowserState* browser_state);
  ~BrowserStateDataRemover();

  // If set then the last username will be removed from the browser state prefs
  // after the data has been wiped.
  void SetForgetLastUsername();

  // Wipes all the data in the browser state and invokes |callback| when done.
  // This can be called only once, and this object deletes itself after invoking
  // the callback.
  void RemoveBrowserStateData(ProceduralBlock callback);

 private:
  void NotifyWithDetails(
      const IOSChromeBrowsingDataRemover::NotificationDetails& details);

  ios::ChromeBrowserState* browser_state_;
  base::scoped_nsprotocol<ProceduralBlock> callback_;
  IOSChromeBrowsingDataRemover::CallbackSubscription callback_subscription_;
  bool forget_last_username_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_BROWSER_STATE_DATA_REMOVER_H_
