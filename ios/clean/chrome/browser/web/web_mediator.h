// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_WEB_WEB_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_WEB_WEB_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}

// An object to provide extensible access to a webState.
// Feature implementations that add functionality to a webState by injecting
// javascript should do so in categories on WebMediator that implement protocols
// for use by components interfacing with the feature and by webState clients
// (for example, view controllers displaying webState's view).
// Category implementations should make use of web_mediator+internals.h to
// access internal APIs for this class.
@interface WebMediator : NSObject

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) web::WebState* webState;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_WEB_WEB_MEDIATOR_H_
