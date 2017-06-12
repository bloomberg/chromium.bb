// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_VIEW_CONTENT_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_WEB_VIEW_CONTENT_TEST_UTIL_H_

#import "ios/web/public/web_state/web_state.h"

namespace web {
namespace test {

// Returns true if there is a web view for |web_state| that contains |text|.
// Otherwise, returns false.
bool IsWebViewContainingText(web::WebState* web_state, const std::string& text);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_VIEW_CONTENT_TEST_UTIL_H_
