// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/app/web_view_interaction_test_util.h"

#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"

namespace chrome_test_util {

void TapWebViewElementWithId(const std::string& element_id) {
  web::test::TapWebViewElementWithId(GetCurrentWebState(), element_id);
}

void SubmitWebViewFormWithId(const std::string& form_id) {
  web::test::SubmitWebViewFormWithId(GetCurrentWebState(), form_id);
}

}  // namespace chrome_test_util
