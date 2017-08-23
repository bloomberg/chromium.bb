// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_CONTEXT_MENU_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_CONTEXT_MENU_DELEGATE_H_

#import <WebKit/WebKit.h>

#import "ios/web/public/web_state/context_menu_params.h"

// Implement this protocol to listen to the custom context menu trigger from
// WKWebView.
@protocol CRWContextMenuDelegate
// Called when the custom Context menu recognizer triggers on |webView|.
// - triggered on a long press gesture, slightly shorter than the default
//   context menu recognizer.
// - if result is YES, the system context many will be suppressed.
//   YES must be returned from the same runloop.
// - if result is NO, the system context many will be displayed.
// - Client can return YES without showing any UI to cancel the system context
//   menu.
// TODO(crbug.com/228179): This class only triggers context menu on mainFrame.
- (BOOL)webView:(WKWebView*)webView
    handleContextMenu:(const web::ContextMenuParams&)params;
@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_CONTEXT_MENU_DELEGATE_H_
