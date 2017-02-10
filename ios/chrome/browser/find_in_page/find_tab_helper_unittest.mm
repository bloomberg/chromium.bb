// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_tab_helper.h"

#include "base/macros.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/web_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for the FindTabHelper class.
class FindTabHelperTest : public web::WebTest {
 public:
  FindTabHelperTest() { FindTabHelper::CreateForWebState(&web_state_, nil); }
  ~FindTabHelperTest() override = default;

 protected:
  web::TestWebState web_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FindTabHelperTest);
};

// Tests that the helper's FindInPageController exists.
TEST_F(FindTabHelperTest, ControllerExists) {
  DCHECK(FindTabHelper::FromWebState(&web_state_)->GetController());
}
