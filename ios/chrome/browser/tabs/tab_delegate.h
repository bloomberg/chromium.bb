// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_DELEGATE_H_

#import <UIKit/UIKit.h>

// A protocol implemented by a delegate of Tab.
@protocol TabDelegate

// Discard prerenderer (e.g. when dialog was suppressed).
- (void)discardPrerender;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_DELEGATE_H_
