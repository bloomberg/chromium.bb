// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_NAVIGATION_UTIL_H_
#define IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_NAVIGATION_UTIL_H_

namespace web {
class WebState;
}  // namespace web

namespace native_app_launcher {

// Returns whether the current state is arrived at via a "link navigation" in
// the sense of Native App Launcher, i.e. a navigation caused by an explicit
// user action in the rectangle of the web content area.
bool IsLinkNavigation(web::WebState* web_state);

}  // namespace native_app_launcher

#endif  // IOS_CHROME_BROWSER_NATIVE_APP_LAUNCHER_NATIVE_APP_NAVIGATION_UTIL_H_
