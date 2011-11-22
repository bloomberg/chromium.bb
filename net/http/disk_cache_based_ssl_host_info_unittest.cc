// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/http/disk_cache_based_ssl_host_info.h"
#include "net/http/mock_http_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

class DeleteSSLHostInfoOldCompletionCallback : public TestOldCompletionCallback {
 public:
  explicit DeleteSSLHostInfoOldCompletionCallback(net::SSLHostInfo* ssl_host_info)
      : ssl_host_info_(ssl_host_info) {}

  virtual void RunWithParams(const Tuple1<int>& params) {
    delete ssl_host_info_;
    TestOldCompletionCallback::RunWithParams(params);
  }

 private:
  net::SSLHostInfo* ssl_host_info_;
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
  net::SSLHostInfo* ssl_host_info =
      new net::DiskCacheBasedSSLHostInfo("https://www.verisign.com", ssl_config,
                                         &cert_verifier, cache.http_cache());
  ssl_host_info->Start();
  DeleteSSLHostInfoOldCompletionCallback callback(ssl_host_info);
  int rv = ssl_host_info->WaitForDataReady(&callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  // Now complete the backend creation and let the callback run.
  factory->FinishCreation();
  EXPECT_EQ(net::OK, callback.GetResult(rv));
}
