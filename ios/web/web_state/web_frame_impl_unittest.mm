// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frame_impl.h"

#import "base/base64.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using crypto::SymmetricKey;

namespace {

const char kFrameId[] = "12345678910ABCDEF12345678910ABCDEF";

// A base64 encoded sample key.
const char kFrameKey[] = "d1uJzdvOFIUT5kEpK4o+x5JCaSlYT/a45ISU7S9EzTo=";

// Returns a key which can be used to create a WebFrame.
std::unique_ptr<SymmetricKey> CreateKey() {
  std::string decoded_frame_key_string;
  base::Base64Decode(kFrameKey, &decoded_frame_key_string);
  return crypto::SymmetricKey::Import(crypto::SymmetricKey::Algorithm::AES,
                                      decoded_frame_key_string);
}

}  // namespace

namespace web {

using WebFrameImplTest = PlatformTest;

// Tests creation of a WebFrame for the main frame.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrame) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, CreateKey(), /*is_main_frame=*/true,
                         security_origin, &test_web_state);

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for a frame which is not the main frame.
TEST_F(WebFrameImplTest, CreateWebFrameForIFrame) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, CreateKey(), /*is_main_frame=*/false,
                         security_origin, &test_web_state);

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_FALSE(web_frame.IsMainFrame());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

}  // namespace web
