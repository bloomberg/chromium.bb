// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

class PagePlaceholderTabHelper;

// Delegate for PagePlaceholderTabHelper.
@protocol PagePlaceholderTabHelperDelegate<NSObject>

// Asks the delegate to display a placeholder over a loading WebState.
- (void)displayPlaceholderForPagePlaceholderTabHelper:
    (PagePlaceholderTabHelper*)tabHelper;

// Asks the delegate to remove any placeholder being displayed.
- (void)removePlaceholderForPagePlaceholderTabHelper:
    (PagePlaceholderTabHelper*)tabHelper;

@end

#endif  // IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_TAB_HELPER_DELEGATE_H_
