// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_WEB_VIEW_INTERACTION_TEST_UTIL_H_
#define IOS_WEB_VIEW_TEST_WEB_VIEW_INTERACTION_TEST_UTIL_H_

@class CWVWebView;
@class NSString;

namespace ios_web_view {
namespace test {

// Returns whether the element with |element_id| in the passed |web_view| has
// been tapped using a JavaScript click() event.
bool TapChromeWebViewElementWithId(CWVWebView* web_view, NSString* element_id);

}  // namespace test
}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_TEST_WEB_VIEW_INTERACTION_TEST_UTIL_H_
