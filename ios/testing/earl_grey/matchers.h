// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_MATCHERS_H_
#define IOS_TESTING_EARL_GREY_MATCHERS_H_

#import <Foundation/Foundation.h>

#import <EarlGrey/EarlGrey.h>

namespace testing {

// Matcher for context menu item whose text is exactly |text|.
id<GREYMatcher> ContextMenuItemWithText(NSString* text);

// Matcher for a UI element to tap to dismiss the the context menu, where
// |cancel_text| is the localized text used for the action sheet cancel control.
// On phones, where the context menu is an action sheet, this will be a matcher
// for the menu item with |cancel_text| as its label.
// On tablets, where the context menu is a popover, this will be a matcher for
// some element outside of the popover.
id<GREYMatcher> ElementToDismissContextMenu(NSString* cancel_text);

}  // namespace testing

#endif  // IOS_TESTING_EARL_GREY_MATCHERS_H_
