// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_CRW_FAKE_WEB_CONTROLLER_OBSERVER_H_
#define IOS_WEB_TEST_CRW_FAKE_WEB_CONTROLLER_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state/crw_web_controller_observer.h"

@interface CRWFakeWebControllerObserver : NSObject<CRWWebControllerObserver>

// YES if the page has loaded.
@property(nonatomic, readonly) BOOL pageLoaded;

@end

#endif // IOS_WEB_TEST_CRW_FAKE_WEB_CONTROLLER_OBSERVER_H_
