// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_UTIL_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_UTIL_H_

#import <UIKit/UIKit.h>

namespace ios_internal {
namespace tab_util {

// Returns a UIBezierPath outlining the chrome tab shape that fits in the given
// |rect|.
UIBezierPath* tabBezierPathForRect(CGRect rect);

}  // namespace tab_util
}  // namespace ios_internal

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_UTIL_H_
