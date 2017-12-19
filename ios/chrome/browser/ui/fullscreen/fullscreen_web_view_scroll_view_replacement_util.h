// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_VIEW_SCROLL_VIEW_REPLACEMENT_UTIL_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_VIEW_SCROLL_VIEW_REPLACEMENT_UTIL_H_

#import <Foundation/Foundation.h>

@protocol CRWWebViewProxy;
class FullscreenModel;

// Updates |proxy|'s content offset and top padding to ensure that the content
// is fully visible under the toolbar when its scroll view is replaced.
void UpdateFullscreenWebViewProxyForReplacedScrollView(
    id<CRWWebViewProxy> proxy,
    FullscreenModel* model);

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_VIEW_SCROLL_VIEW_REPLACEMENT_UTIL_H_
