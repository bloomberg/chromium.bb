// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_user_data.h"

#include <stdint.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "net/base/request_priority.h"
#include "net/nqe/effective_connection_type.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

class PreviewsUserDataTest : public testing::Test {
 public:
  PreviewsUserDataTest() {}
  ~PreviewsUserDataTest() override {}

 private:
  base::MessageLoopForIO message_loop_;
};

TEST_F(PreviewsUserDataTest, TestConstructor) {
  uint64_t id = 5u;
  std::unique_ptr<PreviewsUserData> data(new PreviewsUserData(5u));
  EXPECT_EQ(id, data->page_id());
}

TEST_F(PreviewsUserDataTest, AddToURLRequest) {
  std::unique_ptr<net::URLRequestContext> context(new net::URLRequestContext());
  std::unique_ptr<net::URLRequest> fake_request(context->CreateRequest(
      GURL("http://www.google.com"), net::RequestPriority::IDLE, nullptr,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  PreviewsUserData* data = PreviewsUserData::GetData(*fake_request.get());
  EXPECT_FALSE(data);

  data = PreviewsUserData::Create(fake_request.get(), 1u);
  EXPECT_TRUE(data);
  EXPECT_EQ(data, PreviewsUserData::GetData(*fake_request));

  EXPECT_EQ(data, PreviewsUserData::Create(fake_request.get(), 1u));
}

TEST_F(PreviewsUserDataTest, DeepCopy) {
  uint64_t id = 5u;
  std::unique_ptr<PreviewsUserData> data(new PreviewsUserData(5u));
  EXPECT_EQ(id, data->page_id());

  EXPECT_EQ(id, data->DeepCopy()->page_id());
}

}  // namespace

}  // namespace previews
