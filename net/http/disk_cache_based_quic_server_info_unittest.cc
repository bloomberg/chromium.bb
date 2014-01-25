// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "net/base/net_errors.h"
#include "net/http/disk_cache_based_quic_server_info.h"
#include "net/http/mock_http_cache.h"
#include "net/quic/crypto/quic_server_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is an empty transaction, needed to register the URL and the test mode.
const MockTransaction kHostInfoTransaction = {
  "quicserverinfo:https://www.google.com",
  "",
  base::Time(),
  "",
  net::LOAD_NORMAL,
  "",
  "",
  base::Time(),
  "",
  TEST_MODE_NORMAL,
  NULL,
  0
};

// Tests that we can delete a DiskCacheBasedQuicServerInfo object in a
// completion callback for DiskCacheBasedQuicServerInfo::WaitForDataReady.
TEST(DiskCacheBasedQuicServerInfo, DeleteInCallback) {
  // Use the blocking mock backend factory to force asynchronous completion
  // of quic_server_info->WaitForDataReady(), so that the callback will run.
  MockBlockingBackendFactory* factory = new MockBlockingBackendFactory();
  MockHttpCache cache(factory);
  scoped_ptr<net::QuicServerInfo> quic_server_info(
      new net::DiskCacheBasedQuicServerInfo("https://www.verisign.com",
                                            cache.http_cache()));
  quic_server_info->Start();
  net::TestCompletionCallback callback;
  int rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  // Now complete the backend creation and let the callback run.
  factory->FinishCreation();
  EXPECT_EQ(net::OK, callback.GetResult(rv));
}

// Tests the basic logic of storing, retrieving and updating data.
TEST(DiskCacheBasedQuicServerInfo, Update) {
  MockHttpCache cache;
  AddMockTransaction(&kHostInfoTransaction);
  net::TestCompletionCallback callback;

  scoped_ptr<net::QuicServerInfo> quic_server_info(
      new net::DiskCacheBasedQuicServerInfo("https://www.google.com",
                                            cache.http_cache()));
  quic_server_info->Start();
  int rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  net::QuicServerInfo::State* state = quic_server_info->mutable_state();
  // TODO(rtenneti): Flesh out details of net::QuicServerInfo::State.
  EXPECT_TRUE(state->data.empty());
  state->data.push_back(std::string("foo"));
  quic_server_info->Persist();

  // Wait until Persist() does the work.
  base::MessageLoop::current()->RunUntilIdle();

  // Open the stored net::QuicCryptoClientConfig::CachedState.
  quic_server_info.reset(
      new net::DiskCacheBasedQuicServerInfo("https://www.google.com",
                                            cache.http_cache()));
  quic_server_info->Start();
  rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  // And now update the data.
  state = quic_server_info->mutable_state();
  // TODO(rtenneti): Flesh out details of net::QuicServerInfo::State.
  // Verify the data after we implement save and restore of the data.
  state->data.push_back(std::string("bar"));

  // Fail instead of DCHECKing double creates.
  cache.disk_cache()->set_double_create_check(false);
  quic_server_info->Persist();
  base::MessageLoop::current()->RunUntilIdle();

  // Verify that the state was updated.
  quic_server_info.reset(
      new net::DiskCacheBasedQuicServerInfo("https://www.google.com",
                                            cache.http_cache()));
  quic_server_info->Start();
  rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  state = quic_server_info->mutable_state();
  // TODO(rtenneti): Flesh out details of net::QuicServerInfo::State.
  // Verify the data after we implement save and restore of the data.

  RemoveMockTransaction(&kHostInfoTransaction);
}

}  // namespace
