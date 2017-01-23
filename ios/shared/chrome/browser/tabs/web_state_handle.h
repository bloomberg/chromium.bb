// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_HANDLE_H_
#define IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_HANDLE_H_

namespace web {
class WebState;
class WebStateImpl;
}

// Wraps a web::WebState instance in an Objective-C instance. Allows using
// Objective-C collection to reference web::WebStates.
@protocol WebStateHandle

// Returns the wrapped web::WebState.
@property(nonatomic, readonly) web::WebState* webState;

// Returns the wrapped web::WebState as a web::WebStateImpl. Deprecated,
// use webState instead if possible.
@property(nonatomic, readonly) web::WebStateImpl* webStateImpl;

@end

#endif  // IOS_SHARED_CHROME_BROWSER_TABS_WEB_STATE_HANDLE_H_
