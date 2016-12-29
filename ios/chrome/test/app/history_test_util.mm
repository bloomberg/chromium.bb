// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/history_test_util.h"

#import "base/mac/bind_objc_block.h"
#import "base/test/ios/wait_util.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/main_controller_private.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/ios_chrome_browsing_data_remover.h"
#include "ios/chrome/browser/callback_counter.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace chrome_test_util {

void ClearBrowsingHistory() {
  MainController* main_controller = GetMainController();
  ios::ChromeBrowserState* active_state = GetOriginalBrowserState();
  scoped_refptr<CallbackCounter> callback_counter(
      new CallbackCounter(base::BindBlock(^{
      })));
  callback_counter->IncrementCount();
  __block bool did_complete = false;
  [main_controller
      removeBrowsingDataFromBrowserState:active_state
                                    mask:IOSChromeBrowsingDataRemover::
                                             REMOVE_HISTORY
                              timePeriod:browsing_data::ALL_TIME
                       completionHandler:^{
                         callback_counter->DecrementCount();
                         did_complete = true;
                       }];
  // TODO(crbug.com/631795): This is a workaround that will be removed soon.
  // This code waits for success or timeout, then returns. This needs to be
  // fixed so that failure is correctly marked here, or the caller handles
  // waiting for the operation to complete.
  // Wait for history to be cleared.
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:4.0];
  while (!did_complete &&
         [[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::TimeDelta::FromSecondsD(0.1));
  }
}

}  // namespace chrome_test_util
