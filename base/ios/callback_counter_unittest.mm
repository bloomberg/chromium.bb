// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/callback_counter.h"

#include "base/bind.h"
#include "base/mac/bind_objc_block.h"
#include "base/memory/ref_counted.h"
#import "base/test/ios/wait_util.h"
#include "testing/platform_test.h"

using CallbackCounterTest = PlatformTest;

// Tests that CallbackCounter works with adding callbacks one by one.
TEST_F(CallbackCounterTest, BasicIncrementByOne) {
  __block BOOL block_was_called = NO;
  scoped_refptr<CallbackCounter> callback_counter =
      new CallbackCounter(base::BindBlock(^{
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

// Tests that CallbackCounter works with adding all callbacks at once.
TEST_F(CallbackCounterTest, BasicIncrementByMoreThanOne) {
  __block BOOL block_was_called = NO;
  scoped_refptr<CallbackCounter> callback_counter =
      new CallbackCounter(base::BindBlock(^{
        block_was_called = YES;
      }));

  // Enqueue the 5 callbacks.
  callback_counter->IncrementCount(5);
  for (int i = 0; i < 5; i++) {
    dispatch_async(dispatch_get_main_queue(), ^{
      callback_counter->DecrementCount();
    });
  }
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });
}
