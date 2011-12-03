// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/http/disk_cache_based_ssl_host_info.h"
#include "net/http/mock_http_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is an empty transaction, needed to register the URL and the test mode.
const MockTransaction kHostInfoTransaction = {
  "sslhostinfo:https://www.google.com",
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

// Tests that we can delete a DiskCacheBasedSSLHostInfo object in a
// completion callback for DiskCacheBasedSSLHostInfo::WaitForDataReady.
TEST(DiskCacheBasedSSLHostInfo, DeleteInCallback) {
  net::CertVerifier cert_verifier;
  // Use the blocking mock backend factory to force asynchronous completion
  // of ssl_host_info->WaitForDataReady(), so that the callback will run.
  MockBlockingBackendFactory* factory = new MockBlockingBackendFactory();
  MockHttpCache cache(factory);
  net::SSLConfig ssl_config;
  scoped_ptr<net::SSLHostInfo> ssl_host_info(
      new net::DiskCacheBasedSSLHostInfo("https://www.verisign.com", ssl_config,
                                         &cert_verifier, cache.http_cache()));
  ssl_host_info->Start();
  net::TestCompletionCallback callback;
  int rv = ssl_host_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  // Now complete the backend creation and let the callback run.
  factory->FinishCreation();
  EXPECT_EQ(net::OK, callback.GetResult(rv));
}

// Tests the basic logic of storing, retrieving and updating data.
TEST(DiskCacheBasedSSLHostInfo, Update) {
  MockHttpCache cache;
  AddMockTransaction(&kHostInfoTransaction);
  net::TestCompletionCallback callback;

  // Store a certificate chain.
  net::CertVerifier cert_verifier;
  net::SSLConfig ssl_config;
  scoped_ptr<net::SSLHostInfo> ssl_host_info(
      new net::DiskCacheBasedSSLHostInfo("https://www.google.com", ssl_config,
                                         &cert_verifier, cache.http_cache()));
  ssl_host_info->Start();
  int rv = ssl_host_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  net::SSLHostInfo::State* state = ssl_host_info->mutable_state();
  EXPECT_TRUE(state->certs.empty());
  state->certs.push_back(std::string("foo"));
  ssl_host_info->Persist();

  // Wait until Persist() does the work.
  MessageLoop::current()->RunAllPending();

  // Open the stored certificate chain.
  ssl_host_info.reset(
      new net::DiskCacheBasedSSLHostInfo("https://www.google.com", ssl_config,
                                         &cert_verifier, cache.http_cache()));
  ssl_host_info->Start();
  rv = ssl_host_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  // And now update the data.
  state = ssl_host_info->mutable_state();
  EXPECT_EQ(1U, state->certs.size());
  EXPECT_EQ("foo", state->certs.front());
  state->certs.push_back(std::string("bar"));

  // Fail instead of DCHECKing double creates.
  cache.disk_cache()->set_double_create_check(false);
  ssl_host_info->Persist();
  MessageLoop::current()->RunAllPending();

  // Verify that the state was updated.
  ssl_host_info.reset(
      new net::DiskCacheBasedSSLHostInfo("https://www.google.com", ssl_config,
                                         &cert_verifier, cache.http_cache()));
  ssl_host_info->Start();
  rv = ssl_host_info->WaitForDataReady(callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  state = ssl_host_info->mutable_state();
  EXPECT_EQ(2U, state->certs.size());
  EXPECT_EQ("foo", state->certs[0]);
  EXPECT_EQ("bar", state->certs[1]);

  RemoveMockTransaction(&kHostInfoTransaction);
}

}  // namespace
