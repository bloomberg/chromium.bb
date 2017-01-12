// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_HEADERS_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_HEADERS_DELEGATE_H_

#import <UIKit/UIKit.h>

// A protocol implemented by a delegate of Tab that handles the headers above a
// tab.
@protocol TabHeadersDelegate

// Called to retrieve the height of the header view above |tab|.
- (CGFloat)headerHeightForTab:(Tab*)tab;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_HEADERS_DELEGATE_H_
