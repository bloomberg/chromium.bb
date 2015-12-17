// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/crw_url_verifying_protocol_handler.h"

#include "base/memory/scoped_ptr.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/web_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TODO(shreyasv): See if this can use the WebTest test fixture.
TEST(CRWURLVerifyingProtocolHandlerTest, NonLazyInitializer) {
  web::ScopedTestingWebClient web_client(
      make_scoped_ptr(new web::TestWebClient));
  EXPECT_TRUE([CRWURLVerifyingProtocolHandler preInitialize]);
}

}  // namespace
