// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frame_impl.h"

#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kFrameId[] = "frameId";
}  // namespace

namespace web {

using WebFrameImplTest = PlatformTest;

// Tests creation of a WebFrame for the main frame.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrame) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/true, security_origin,
                         &test_web_state);

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for a frame which is not the main frame.
TEST_F(WebFrameImplTest, CreateWebFrameForIFrame) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_FALSE(web_frame.IsMainFrame());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

}  // namespace web
