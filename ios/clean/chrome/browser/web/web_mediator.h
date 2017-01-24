// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_WEB_WEB_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_WEB_WEB_MEDIATOR_H_

#import <Foundation/Foundation.h>
#include <memory>

#import "ios/shared/chrome/browser/tabs/web_state_handle.h"

namespace ios {
class ChromeBrowserState;
}

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
@interface WebMediator : NSObject<WebStateHandle>

// Creates a new, empty webState object for |browserState| and returns a
// WebMediator instance for it.
+ (instancetype)webMediatorForBrowserState:
    (ios::ChromeBrowserState*)browserState;

// Creates a new mediator for |webState|, taking ownership of it.
- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
    NS_DESIGNATED_INITIALIZER;

// Creates a mediator with no webState, for use in testing classes that operate
// on WebMediators.
- (instancetype)init;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_WEB_WEB_MEDIATOR_H_
