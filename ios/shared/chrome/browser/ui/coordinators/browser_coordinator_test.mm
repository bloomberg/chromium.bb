// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator_test.h"

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserCoordinatorTest::BrowserCoordinatorTest() {
  // Initialize the browser.
  TestChromeBrowserState::Builder builder;
  chrome_browser_state_ = builder.Build();
  browser_ = base::MakeUnique<Browser>(chrome_browser_state_.get());
}

BrowserCoordinatorTest::~BrowserCoordinatorTest() = default;
