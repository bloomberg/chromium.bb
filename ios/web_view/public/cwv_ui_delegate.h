// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_UI_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_UI_DELEGATE_H_

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

@class CWVHTMLElement;
@class CWVWebView;

// UI delegate interface for a CWVWebView.  Embedders can implement the
// functions in order to customize library behavior.
@protocol CWVUIDelegate<NSObject>

@optional
// Instructs the delegate to present context menu in response to user’s long
// press gesture at |location| in |view| coordinate space. |element| is an HTML
// element which received the gesture.
- (void)webView:(CWVWebView*)webView
    runContextMenuWithTitle:(NSString*)menuTitle
             forHTMLElement:(CWVHTMLElement*)element
                     inView:(UIView*)view
        userGestureLocation:(CGPoint)location;

// Instructs the delegate to show UI in response to window.alert JavaScript
// call.
- (void)webView:(CWVWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                               pageURL:(NSURL*)URL
                     completionHandler:(void (^)(void))completionHandler;

// Instructs the delegate to show UI in response to window.confirm JavaScript
// call.
- (void)webView:(CWVWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                                 pageURL:(NSURL*)URL
                       completionHandler:(void (^)(BOOL))completionHandler;

// Instructs the delegate to show UI in response to window.prompt JavaScript
// call.
- (void)webView:(CWVWebView*)webView
    runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
                              defaultText:(NSString*)defaultText
                                  pageURL:(NSURL*)URL
                        completionHandler:
                            (void (^)(NSString*))completionHandler;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_UI_DELEGATE_H_
