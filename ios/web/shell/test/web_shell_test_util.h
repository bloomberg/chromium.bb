// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_WEB_SHELL_TEST_UTIL_H_
#define IOS_WEB_SHELL_TEST_WEB_SHELL_TEST_UTIL_H_

#import "ios/web/shell/view_controller.h"

namespace web {
namespace web_shell_test_util {

// Gets the current ViewController for the web shell.
ViewController* GetCurrentViewController();

}  // namespace web_shell_test_util
}  // namespace web

#endif  // IOS_WEB_SHELL_TEST_WEB_SHELL_TEST_UTIL_H_