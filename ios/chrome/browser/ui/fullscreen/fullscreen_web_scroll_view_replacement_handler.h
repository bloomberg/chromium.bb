// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_SCROLL_VIEW_REPLACEMENT_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_SCROLL_VIEW_REPLACEMENT_HANDLER_H_

#import <Foundation/Foundation.h>

@protocol CRWWebViewProxy;
class FullscreenModel;

// Helper object that listens for scroll view replacements on the web view proxy
// and updates the content offset of the new scroll view so that its content is
// below the toolbar.
@interface FullscreenWebScrollViewReplacementHandler : NSObject

// The proxy being observed.
@property(nonatomic, weak, nullable) id<CRWWebViewProxy> proxy;

// Designated initializer for an observer that uses |model| to update its proxy.
- (nullable instancetype)initWithModel:(nonnull FullscreenModel*)model
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_SCROLL_VIEW_REPLACEMENT_HANDLER_H_
