// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_chrome_application_mac.h"

#include "testing/gtest/include/gtest/gtest.h"

@implementation MockCrControlApp

- (BOOL)isHandlingSendEvent {
  return isHandlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  isHandlingSendEvent_ = handlingSendEvent;
}

@end

namespace mock_cr_app {

void RegisterMockCrControlApp() {
  NSApplication* app = [MockCrControlApp sharedApplication];
  ASSERT_TRUE([app conformsToProtocol:@protocol(CrAppControlProtocol)]);
}

}  // namespace mock_cr_app
