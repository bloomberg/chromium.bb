// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_TOP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_UTIL_TOP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace top_view_controller {

UIViewController* TopPresentedViewControllerFrom(
    UIViewController* base_view_controller);

UIViewController* TopPresentedViewController();

}  // namespace top_view_controller

#endif  // IOS_CHROME_BROWSER_UI_UTIL_TOP_VIEW_CONTROLLER_H_
