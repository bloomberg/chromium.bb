// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_WEB_VIEW_INTERACTION_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_WEB_VIEW_INTERACTION_TEST_UTIL_H_

#include <string>

namespace chrome_test_util {

// Attempts to tap the element with |element_id| in the current WebState
// using a JavaScript click() event.
void TapWebViewElementWithId(const std::string& element_id);

// Attemps to submit form with |form_id| in the current WebState.
void SubmitWebViewFormWithId(const std::string& form_id);

}  //  namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_WEB_VIEW_INTERACTION_TEST_UTIL_H_
