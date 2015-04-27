// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_CONTROLLER_OBSERVER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_CONTROLLER_OBSERVER_H_

#import <Foundation/Foundation.h>
#include <string>

@class CRWWebController;
@protocol CRWWebViewProxy;
class GURL;
@class UIWebView;

namespace base {
class DictionaryValue;
}

// NOTE: When adding new methods to CRWWebControllerObserver, consider adding
// them to WebStateObserver instead if they need to be surfaced to the public
// API.
@protocol CRWWebControllerObserver<NSObject>

@optional

// Supplies a text prefix to the CRWWebController to indicate which commands the
// observer should receive using the handleCommand message.
// Called only as the observer is added to its parent CRWWebController.
@property(nonatomic, readonly) NSString* commandPrefix;

// Called when the current page is loaded.
// DEPRECATED: Use WebStateObserver instead.
- (void)pageLoaded:(CRWWebController*)webController;

// Called when the web controller is about to close.
- (void)webControllerWillClose:(CRWWebController*)webController;

// Handle the command from page scripts. Return NO if the command was known to
// be invalid. This will cause the page to be reset as a security precaution.
// DEPRECATED: Use WebState::ScriptCommandCallback instead.
- (BOOL)handleCommand:(const base::DictionaryValue&)command
        webController:(CRWWebController*)webController
    userIsInteracting:(BOOL)userIsInteracting
            originURL:(const GURL&)originURL;

// Gives CRWWebControllerObservers access to the CRWWebViewProxy.
- (void)setWebViewProxy:(id<CRWWebViewProxy>)webView
             controller:(CRWWebController*)webController;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_CONTROLLER_OBSERVER_H_
