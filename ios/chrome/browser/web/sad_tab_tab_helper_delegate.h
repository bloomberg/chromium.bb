// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SAD_TAB_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_WEB_SAD_TAB_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

class SadTabTabHelper;

// SadTabTabHelperDelegate defines an interface that allows SadTabTabHelper
// instances to learn about the visibility of the Tab they are helping.
@protocol SadTabTabHelperDelegate<NSObject>

// Returns whether the tab for the tab helper is visible - i.e. the tab is
// frontmost and the application is in an active state - allowing differences
// in the behavior of the helper for visible vs. non-visible tabs.
- (BOOL)isTabVisibleForTabHelper:(SadTabTabHelper*)tabHelper;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_CHROME_BROWSER_WEB_SAD_TAB_TAB_HELPER_DELEGATE_H_
