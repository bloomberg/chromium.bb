// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_CHROME_APPLICATION_MAC_H_
#define CONTENT_TEST_MOCK_CHROME_APPLICATION_MAC_H_
#pragma once

#if defined(__OBJC__)

#import <AppKit/AppKit.h>

#include "content/common/mac/scoped_sending_event.h"

// Mock implementation supporting CrAppControlProtocol.  Can be used
// in tests using ScopedSendingEvent to deal with nested message
// loops.  Also implements CrAppProtocol so can be used as a
// replacement for MockCrApp (in base/).
@interface MockCrControlApp : NSApplication<CrAppControlProtocol> {
 @private
  BOOL isHandlingSendEvent_;
}
@end

#endif  // __OBJC__

// To be used to instantiate MockCrControlApp from C++ code.
namespace mock_cr_app {
void RegisterMockCrControlApp();
}  // namespace mock_cr_app

#endif  // CONTENT_TEST_MOCK_CHROME_APPLICATION_MAC_H_
