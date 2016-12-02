// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_NATIVE_CONTENT_H_
#define IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_NATIVE_CONTENT_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/block_types.h"
#include "url/gurl.h"

@protocol CRWNativeContentDelegate;

// Abstract methods needed for manipulating native content in the web content
// area.
@protocol CRWNativeContent<NSObject>

// The page title, meant for display to the user. Will return nil if not
// available.
- (NSString*)title;

// The URL represented by the content being displayed.
- (const GURL&)url;

// The view to insert into the content area displayed to the user.
- (UIView*)view;

// Called when memory is low. Release anything (such as views) that can be
// easily re-created to free up RAM.
- (void)handleLowMemory;

// Returns YES if there is currently a live view in the tab (e.g., the view
// hasn't been discarded due to low memory).
// NOTE: This should be used for metrics-gathering only; for any other purpose
// callers should not know or care whether the view is live.
- (BOOL)isViewAlive;

// Reload any displayed data to ensure the view is up to date.
- (void)reload;

@optional

// Optional method that allows to set CRWNativeContent delegate.
- (void)setDelegate:(id<CRWNativeContentDelegate>)delegate;

// Notifies the CRWNativeContent that it has been shown.
- (void)wasShown;

// Notifies the CRWNativeContent that it has been hidden.
- (void)wasHidden;

// Executes JavaScript on the native view. |handler| is called with the results
// of the evaluation. If the native view cannot evaluate JS at the moment,
// |handler| is called with an NSError.
- (void)executeJavaScript:(NSString*)script
        completionHandler:(web::JavaScriptResultBlock)handler;

// Returns |YES| if CRWNativeContent wants the keyboard shield when the keyboard
// is up.
- (BOOL)wantsKeyboardShield;

// Returns |YES| if CRWNativeContent wants the hint text displayed.
// TODO(crbug.com/374984): Remove this. This is chrome level concept and should
// not exist in the web/ layer.
- (BOOL)wantsLocationBarHintText;

// Dismisses on-screen keyboard if necessary.
- (void)dismissKeyboard;

// Dismisses any outstanding modal interaction elements (e.g. modal view
// controllers, context menus, etc).
- (void)dismissModals;

// A native content controller should do any clean up at this time when
// WebController closes.
- (void)close;

// Enables or disables usage of web views inside the native controller.
- (void)setWebUsageEnabled:(BOOL)webUsageEnabled;

// Enables or disables the scrolling in the native view (when available).
- (void)setScrollEnabled:(BOOL)enabled;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// The URL that will be displayed to the user when presenting this native
// content.
- (GURL)virtualURL;

@end

// CRWNativeContent delegate protocol.
@protocol CRWNativeContentDelegate<NSObject>

@optional
// Called when the content supplies a new title.
- (void)nativeContent:(id)content titleDidChange:(NSString*)title;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_NATIVE_CONTENT_H_
