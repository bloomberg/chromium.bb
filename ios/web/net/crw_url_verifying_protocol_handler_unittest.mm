// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/crw_url_verifying_protocol_handler.h"

#include "base/memory/scoped_ptr.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/web_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TODO(shreyasv): See if this can use the WebTest test fixture.
TEST(CRWURLVerifyingProtocolHandlerTest, NonLazyInitializer) {
  scoped_ptr<web::TestWebClient> test_web_client(new web::TestWebClient());
  web::SetWebClient(test_web_client.get());
  EXPECT_TRUE([CRWURLVerifyingProtocolHandler preInitialize]);
  web::SetWebClient(nullptr);
}

}  // namespace
