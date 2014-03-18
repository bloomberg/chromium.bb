// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/disk_cache_based_quic_server_info.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "net/base/net_errors.h"
#include "net/http/mock_http_cache.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/quic_session_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is an empty transaction, needed to register the URL and the test mode.
const MockTransaction kHostInfoTransaction1 = {
  "quicserverinfo:https://www.google.com:443",
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

const MockTransaction kHostInfoTransaction2 = {
  "quicserverinfo:http://www.google.com:80",
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
  net::QuicSessionKey server_key("www.verisign.com", 443, true);
  scoped_ptr<net::QuicServerInfo> quic_server_info(
      new net::DiskCacheBasedQuicServerInfo(server_key, cache.http_cache()));
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
  AddMockTransaction(&kHostInfoTransaction1);
  net::TestCompletionCallback callback;

  net::QuicSessionKey server_key("www.google.com", 443, true);
  scoped_ptr<net::QuicServerInfo> quic_server_info(
      new net::DiskCacheBasedQuicServerInfo(server_key, cache.http_cache()));
  quic_server_info->Start();
  int rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  net::QuicServerInfo::State* state = quic_server_info->mutable_state();
  EXPECT_TRUE(state->certs.empty());
  const string server_config_a = "server_config_a";
  const string source_address_token_a = "source_address_token_a";
  const string server_config_sig_a = "server_config_sig_a";
  const string cert_a = "cert_a";
  const string cert_b = "cert_b";

  state->server_config = server_config_a;
  state->source_address_token = source_address_token_a;
  state->server_config_sig = server_config_sig_a;
  state->certs.push_back(cert_a);
  quic_server_info->Persist();

  // Wait until Persist() does the work.
  base::MessageLoop::current()->RunUntilIdle();

  // Open the stored QuicServerInfo.
  quic_server_info.reset(
      new net::DiskCacheBasedQuicServerInfo(server_key, cache.http_cache()));
  quic_server_info->Start();
  rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  // And now update the data.
  state = quic_server_info->mutable_state();
  state->certs.push_back(cert_b);

  // Fail instead of DCHECKing double creates.
  cache.disk_cache()->set_double_create_check(false);
  quic_server_info->Persist();
  base::MessageLoop::current()->RunUntilIdle();

  // Verify that the state was updated.
  quic_server_info.reset(
      new net::DiskCacheBasedQuicServerInfo(server_key, cache.http_cache()));
  quic_server_info->Start();
  rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  const net::QuicServerInfo::State& state1 = quic_server_info->state();
  EXPECT_TRUE(quic_server_info->IsDataReady());
  EXPECT_EQ(server_config_a, state1.server_config);
  EXPECT_EQ(source_address_token_a, state1.source_address_token);
  EXPECT_EQ(server_config_sig_a, state1.server_config_sig);
  EXPECT_EQ(2U, state1.certs.size());
  EXPECT_EQ(cert_a, state1.certs[0]);
  EXPECT_EQ(cert_b, state1.certs[1]);

  RemoveMockTransaction(&kHostInfoTransaction1);
}

// Test that demonstrates different info is returned when the ports differ.
TEST(DiskCacheBasedQuicServerInfo, UpdateDifferentPorts) {
  MockHttpCache cache;
  AddMockTransaction(&kHostInfoTransaction1);
  AddMockTransaction(&kHostInfoTransaction2);
  net::TestCompletionCallback callback;

  // Persist data for port 443.
  net::QuicSessionKey server_key1("www.google.com", 443, true);
  scoped_ptr<net::QuicServerInfo> quic_server_info1(
      new net::DiskCacheBasedQuicServerInfo(server_key1, cache.http_cache()));
  quic_server_info1->Start();
  int rv = quic_server_info1->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  net::QuicServerInfo::State* state1 = quic_server_info1->mutable_state();
  EXPECT_TRUE(state1->certs.empty());
  const string server_config_a = "server_config_a";
  const string source_address_token_a = "source_address_token_a";
  const string server_config_sig_a = "server_config_sig_a";
  const string cert_a = "cert_a";

  state1->server_config = server_config_a;
  state1->source_address_token = source_address_token_a;
  state1->server_config_sig = server_config_sig_a;
  state1->certs.push_back(cert_a);
  quic_server_info1->Persist();

  // Wait until Persist() does the work.
  base::MessageLoop::current()->RunUntilIdle();

  // Persist data for port 80.
  net::QuicSessionKey server_key2("www.google.com", 80, false);
  scoped_ptr<net::QuicServerInfo> quic_server_info2(
      new net::DiskCacheBasedQuicServerInfo(server_key2, cache.http_cache()));
  quic_server_info2->Start();
  rv = quic_server_info2->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  net::QuicServerInfo::State* state2 = quic_server_info2->mutable_state();
  EXPECT_TRUE(state2->certs.empty());
  const string server_config_b = "server_config_b";
  const string source_address_token_b = "source_address_token_b";
  const string server_config_sig_b = "server_config_sig_b";
  const string cert_b = "cert_b";

  state2->server_config = server_config_b;
  state2->source_address_token = source_address_token_b;
  state2->server_config_sig = server_config_sig_b;
  state2->certs.push_back(cert_b);
  quic_server_info2->Persist();

  // Wait until Persist() does the work.
  base::MessageLoop::current()->RunUntilIdle();

  // Verify the stored QuicServerInfo for port 443.
  scoped_ptr<net::QuicServerInfo> quic_server_info(
      new net::DiskCacheBasedQuicServerInfo(server_key1, cache.http_cache()));
  quic_server_info->Start();
  rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  const net::QuicServerInfo::State& state_a = quic_server_info->state();
  EXPECT_TRUE(quic_server_info->IsDataReady());
  EXPECT_EQ(server_config_a, state_a.server_config);
  EXPECT_EQ(source_address_token_a, state_a.source_address_token);
  EXPECT_EQ(server_config_sig_a, state_a.server_config_sig);
  EXPECT_EQ(1U, state_a.certs.size());
  EXPECT_EQ(cert_a, state_a.certs[0]);

  // Verify the stored QuicServerInfo for port 80.
  quic_server_info.reset(
      new net::DiskCacheBasedQuicServerInfo(server_key2, cache.http_cache()));
  quic_server_info->Start();
  rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  const net::QuicServerInfo::State& state_b = quic_server_info->state();
  EXPECT_TRUE(quic_server_info->IsDataReady());
  EXPECT_EQ(server_config_b, state_b.server_config);
  EXPECT_EQ(source_address_token_b, state_b.source_address_token);
  EXPECT_EQ(server_config_sig_b, state_b.server_config_sig);
  EXPECT_EQ(1U, state_b.certs.size());
  EXPECT_EQ(cert_b, state_b.certs[0]);

  RemoveMockTransaction(&kHostInfoTransaction2);
  RemoveMockTransaction(&kHostInfoTransaction1);
}

// Test IsDataReady when there is a pending write.
TEST(DiskCacheBasedQuicServerInfo, IsDataReady) {
  MockHttpCache cache;
  AddMockTransaction(&kHostInfoTransaction1);
  net::TestCompletionCallback callback;

  net::QuicSessionKey server_key("www.google.com", 443, true);
  scoped_ptr<net::QuicServerInfo> quic_server_info(
      new net::DiskCacheBasedQuicServerInfo(server_key, cache.http_cache()));
  quic_server_info->Start();
  EXPECT_TRUE(quic_server_info->IsDataReady());
  int rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_TRUE(quic_server_info->IsDataReady());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  net::QuicServerInfo::State* state = quic_server_info->mutable_state();
  EXPECT_TRUE(state->certs.empty());
  const string server_config_a = "server_config_a";
  const string source_address_token_a = "source_address_token_a";
  const string server_config_sig_a = "server_config_sig_a";
  const string cert_a = "cert_a";

  state->server_config = server_config_a;
  state->source_address_token = source_address_token_a;
  state->server_config_sig = server_config_sig_a;
  state->certs.push_back(cert_a);
  EXPECT_TRUE(quic_server_info->IsDataReady());
  quic_server_info->Persist();

  // Once we call Persist, IsDataReady should return false until Persist has
  // completed.
  EXPECT_FALSE(quic_server_info->IsDataReady());

  // Wait until Persist() does the work.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(quic_server_info->IsDataReady());

  // Verify that the state was updated.
  quic_server_info.reset(
      new net::DiskCacheBasedQuicServerInfo(server_key, cache.http_cache()));
  quic_server_info->Start();
  rv = quic_server_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  EXPECT_TRUE(quic_server_info->IsDataReady());

  const net::QuicServerInfo::State& state1 = quic_server_info->state();
  EXPECT_TRUE(quic_server_info->IsDataReady());
  EXPECT_EQ(server_config_a, state1.server_config);
  EXPECT_EQ(source_address_token_a, state1.source_address_token);
  EXPECT_EQ(server_config_sig_a, state1.server_config_sig);
  EXPECT_EQ(1U, state1.certs.size());
  EXPECT_EQ(cert_a, state1.certs[0]);

  RemoveMockTransaction(&kHostInfoTransaction1);
}

}  // namespace
