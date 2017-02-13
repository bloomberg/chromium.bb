// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/callback_counter.h"

#include "base/bind.h"
#include "base/mac/bind_objc_block.h"
#include "base/memory/ref_counted.h"
#import "base/test/ios/wait_util.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests that CallbackCounter works with 2 callbacks.
TEST(CallbackCounterTest, Basic) {
  __block BOOL block_was_called = NO;
  scoped_refptr<CallbackCounter> callback_counter =
      new CallbackCounter(base::BindBlockArc(^{
        block_was_called = YES;
      }));

  // Enqueue the first callback.
  callback_counter->IncrementCount();
  dispatch_async(dispatch_get_main_queue(), ^{
    callback_counter->DecrementCount();
  });

  // Enqueue the second callback.
  callback_counter->IncrementCount();
  dispatch_async(dispatch_get_main_queue(), ^{
    callback_counter->DecrementCount();
  });

  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });
}
