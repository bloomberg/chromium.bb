// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/web_shell_test_util.h"

#include <UIKit/UIKit.h>

namespace web {
namespace web_shell_test_util {

ViewController* GetCurrentViewController() {
  return (ViewController*)[[[[UIApplication sharedApplication] delegate] window]
      rootViewController];
}

}  // namespace web_shell_test_util
}  // namespace web
