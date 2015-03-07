// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_USER_INTERFACE_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_USER_INTERFACE_DELEGATE_H_

#include <Foundation/Foundation.h>

@class CRWWebController;
class GURL;

@protocol CRWWebUserInterfaceDelegate<NSObject>

 @optional
// The JavaScript panel selectors below are only called by the web controller
// for builds with WKWebView enabled.

// Displays a JavaScript alert with an OK button, showing the provided message.
// |completionHandler| is called afer the OK button on the alert is tapped.  If
// this selector isn't implemented, the completion handler provided by the web
// view will be called without any UI displayed.
- (void)webController:(CRWWebController*)webController
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                            requestURL:(const GURL&)requestURL
                     completionHandler:(void (^)(void))completionHandler;

// Displays a JavaScript confirm alert with an OK and Cancel button, showing the
// provided message.  |completionHandler| is called after a button is pressed,
// with |isConfirmed| indicating whether OK was pressed.  If this selector isn't
// implemented, the competion handler provided by the web view will be called
// with |isConfirmed| = NO.
- (void)webController:(CRWWebController*)webController
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                              requestURL:(const GURL&)requestURL
                       completionHandler:
        (void (^)(BOOL isConfirmed))completionHandler;

// Displays a JavaScript input alert with an OK and Cancel button, showing the
// provided message and placeholder text.  |completionHandler| is called after a
// button is pressed.  If the OK button is pressed, |input| contains the user
// text.  If the cancel but is pressed, |input| will be nil.  If this selector
// isn't implemented, the completion handler provided by the web view will be
// called with |input| = nil.
- (void)webController:(CRWWebController*)webController
    runJavaScriptTextInputPanelWithPrompt:(NSString*)message
                          placeholderText:(NSString*)placeholderText
                               requestURL:(const GURL&)requestURL
                        completionHandler:
        (void (^)(NSString* input))completionHandler;

// Displays a context menu for DOM element. |point| and |view| represent the
// location and UIView where the context menu was triggered by a user gesture.
// |menuInfo| keys are defined in crw_context_menu_provider.h.
// TODO(eugenebut): create DOMElement class (tag + attributes) and pass
// it and referrer as separate arguments instead of |menuInfo|.
- (void)webController:(CRWWebController*)webController
       runContextMenu:(NSDictionary*)menuInfo
              atPoint:(CGPoint)point
               inView:(UIView*)view;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_CRW_WEB_USER_INTERFACE_DELEGATE_H_
