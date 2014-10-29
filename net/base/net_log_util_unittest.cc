// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_util.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Make sure GetNetConstants doesn't crash.
TEST(NetLogUtil, GetNetConstants) {
  scoped_ptr<base::Value> constants(GetNetConstants());
}

// Make sure GetNetInfo doesn't crash when called on contexts with and without
// caches, and they have the same number of elements.
TEST(NetLogUtil, GetNetInfo) {
  TestURLRequestContext context;
  net::HttpCache* http_cache = context.http_transaction_factory()->GetCache();

  // Get NetInfo when there's no cache backend (It's only created on first use).
  EXPECT_FALSE(http_cache->GetCurrentBackend());
  scoped_ptr<base::DictionaryValue> net_info_without_cache(
      GetNetInfo(&context, NET_INFO_ALL_SOURCES));
  EXPECT_FALSE(http_cache->GetCurrentBackend());
  EXPECT_GT(net_info_without_cache->size(), 0u);

  // Fore creation of a cache backend, and get NetInfo again.
  disk_cache::Backend* backend = NULL;
  EXPECT_EQ(
      OK,
      context.http_transaction_factory()->GetCache()->GetBackend(
          &backend, TestCompletionCallback().callback()));
  EXPECT_TRUE(http_cache->GetCurrentBackend());
  scoped_ptr<base::DictionaryValue> net_info_with_cache(
      GetNetInfo(&context, NET_INFO_ALL_SOURCES));
  EXPECT_GT(net_info_with_cache->size(), 0u);

  EXPECT_EQ(net_info_without_cache->size(), net_info_with_cache->size());
}

}  // namespace

}  // namespace net
