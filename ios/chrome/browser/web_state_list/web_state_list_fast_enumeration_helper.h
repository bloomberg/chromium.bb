// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_FAST_ENUMERATION_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_FAST_ENUMERATION_HELPER_H_

#import <Foundation/Foundation.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"

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
class WebStateListFastEnumerationHelper {
 public:
  WebStateListFastEnumerationHelper(WebStateList* web_state_list,
                                    id<WebStateProxyFactory> proxy_factory);
  ~WebStateListFastEnumerationHelper();

  id<NSFastEnumeration> GetFastEnumeration();

 private:
  base::scoped_nsprotocol<id<NSFastEnumeration>> fast_enumeration_;

  DISALLOW_COPY_AND_ASSIGN(WebStateListFastEnumerationHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_FAST_ENUMERATION_HELPER_H_
