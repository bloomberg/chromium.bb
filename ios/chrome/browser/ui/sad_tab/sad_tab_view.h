// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAD_TAB_SAD_TAB_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SAD_TAB_SAD_TAB_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

// The view used to show "sad tab" content to the user when WKWebView's renderer
// process crashes.
@interface SadTabView : UIView

// Designated initializer.  |reloadHandler| will be called when the reload
// button is tapped and must not be nil.
- (instancetype)initWithReloadHandler:(ProceduralBlock)reloadHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAD_TAB_SAD_TAB_VIEW_H_
