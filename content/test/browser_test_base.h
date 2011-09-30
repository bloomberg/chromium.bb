// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_BROWSER_TEST_BASE_H_
#define CONTENT_TEST_BROWSER_TEST_BASE_H_
#pragma once

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserTestBase : public testing::Test {
 public:
  BrowserTestBase();
  virtual ~BrowserTestBase();

  // We do this so we can be used in a Task.
  void AddRef() {}
  void Release() {}
  static bool ImplementsThreadSafeReferenceCounting() { return false; }

  // Configures everything for an in process browser test, then invokes
  // BrowserMain. BrowserMain ends up invoking RunTestOnMainThreadLoop.
  virtual void SetUp() OVERRIDE;

  // Restores state configured in SetUp.
  virtual void TearDown() OVERRIDE;

 protected:
  // We need these special methods because SetUp is the bottom of the stack
  // that winds up calling your test method, so it is not always an option
  // to do what you want by overriding it and calling the superclass version.
  //
  // Override this for things you would normally override SetUp for. It will be
  // called before your individual test fixture method is run, but after most
  // of the overhead initialization has occured.
  virtual void SetUpInProcessBrowserTestFixture() {}

  // Override this for things you would normally override TearDown for.
  virtual void TearDownInProcessBrowserTestFixture() {}

  // Override this rather than TestBody.
  virtual void RunTestOnMainThread() = 0;

  // This is invoked from main after browser_init/browser_main have completed.
  // This prepares for the test by creating a new browser, runs the test
  // (RunTestOnMainThread), quits the browsers and returns.
  virtual void RunTestOnMainThreadLoop() = 0;

 private:
  void ProxyRunTestOnMainThreadLoop();
};

#endif  // CONTENT_TEST_BROWSER_TEST_BASE_H_
