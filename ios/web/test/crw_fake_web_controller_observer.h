// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_CRW_FAKE_WEB_CONTROLLER_OBSERVER_H_
#define IOS_WEB_TEST_CRW_FAKE_WEB_CONTROLLER_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_vector.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"

namespace base {
class DictionaryValue;
}

// Fake CRWWebControllerObserver. An OCMock is not used because it cannot handle
// C++ types without ugly workarounds, e.g. OCMockComplexTypeHelper/onSelector,
// that effectively employ a 'fake' pattern anyway.
// WCO uses C++ types (e.g. GURL, DictionaryValue) extensively.
@interface CRWFakeWebControllerObserver : NSObject<CRWWebControllerObserver>

// Designated initializer. Returns a CRWWebControllerObserver that claims
// commands with the prefix |commandPrefix|. |commandPrefix| cannot be nil.
- (instancetype)initWithCommandPrefix:(NSString*)commandPrefix;

// YES if the page has loaded.
@property(nonatomic, readonly) BOOL pageLoaded;

// The list of commands that have been received and saved.
- (ScopedVector<base::DictionaryValue>&)commandsReceived;

@end

#endif // IOS_WEB_TEST_CRW_FAKE_WEB_CONTROLLER_OBSERVER_H_
