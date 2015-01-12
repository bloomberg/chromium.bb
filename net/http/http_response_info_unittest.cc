// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_info.h"

#include "base/pickle.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

class HttpResponseInfoTest : public testing::Test {
 protected:
  void SetUp() override {
    response_info_.headers = new net::HttpResponseHeaders("");
  }

  void PickleAndRestore(const net::HttpResponseInfo& response_info,
                        net::HttpResponseInfo* restored_response_info) const {
    Pickle pickle;
    response_info.Persist(&pickle, false, false);
    bool truncated = false;
    restored_response_info->InitFromPickle(pickle, &truncated);
  }

  net::HttpResponseInfo response_info_;
};

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchDefault) {
  EXPECT_FALSE(response_info_.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchCopy) {
  response_info_.unused_since_prefetch = true;
  net::HttpResponseInfo response_info_clone(response_info_);
  EXPECT_TRUE(response_info_clone.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchPersistFalse) {
  net::HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchPersistTrue) {
  response_info_.unused_since_prefetch = true;
  net::HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.unused_since_prefetch);
}
