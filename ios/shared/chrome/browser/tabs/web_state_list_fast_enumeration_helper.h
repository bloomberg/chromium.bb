// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_FAST_ENUMERATION_HELPER_H_
#define IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_FAST_ENUMERATION_HELPER_H_

#import <Foundation/Foundation.h>

class WebStateList;

namespace web {
class WebState;
}

// Protocol used to get Objective-C proxies for WebStates.
@protocol WebStateProxyFactory

// Returns an Objective-C object corresponding to a given WebState.
- (id)proxyForWebState:(web::WebState*)webState;

@end

// Helper class allowing NSFastEnumeration over a WebStateList.
@interface WebStateListFastEnumerationHelper : NSObject<NSFastEnumeration>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                        proxyFactory:(id<WebStateProxyFactory>)proxyFactory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_LIST_FAST_ENUMERATION_HELPER_H_
