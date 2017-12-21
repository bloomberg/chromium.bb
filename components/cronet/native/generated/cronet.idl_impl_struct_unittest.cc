// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#include "components/cronet/native/generated/cronet.idl_c.h"

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

class CronetStructTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  CronetStructTest() {}
  ~CronetStructTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CronetStructTest);
};

// Test Struct Cronet_Exception setters and getters.
TEST_F(CronetStructTest, TestCronet_Exception) {
  Cronet_ExceptionPtr first = Cronet_Exception_Create();
  Cronet_ExceptionPtr second = Cronet_Exception_Create();

  // Copy values from |first| to |second|.
  Cronet_Exception_set_error_code(second,
                                  Cronet_Exception_get_error_code(first));
  EXPECT_EQ(Cronet_Exception_get_error_code(first),
            Cronet_Exception_get_error_code(second));
  Cronet_Exception_set_internal_error_code(
      second, Cronet_Exception_get_internal_error_code(first));
  EXPECT_EQ(Cronet_Exception_get_internal_error_code(first),
            Cronet_Exception_get_internal_error_code(second));
  Cronet_Exception_set_immediately_retryable(
      second, Cronet_Exception_get_immediately_retryable(first));
  EXPECT_EQ(Cronet_Exception_get_immediately_retryable(first),
            Cronet_Exception_get_immediately_retryable(second));
  Cronet_Exception_set_quic_detailed_error_code(
      second, Cronet_Exception_get_quic_detailed_error_code(first));
  EXPECT_EQ(Cronet_Exception_get_quic_detailed_error_code(first),
            Cronet_Exception_get_quic_detailed_error_code(second));
  Cronet_Exception_Destroy(first);
  Cronet_Exception_Destroy(second);
}

// Test Struct Cronet_QuicHint setters and getters.
TEST_F(CronetStructTest, TestCronet_QuicHint) {
  Cronet_QuicHintPtr first = Cronet_QuicHint_Create();
  Cronet_QuicHintPtr second = Cronet_QuicHint_Create();

  // Copy values from |first| to |second|.
  Cronet_QuicHint_set_host(second, Cronet_QuicHint_get_host(first));
  EXPECT_STREQ(Cronet_QuicHint_get_host(first),
               Cronet_QuicHint_get_host(second));
  Cronet_QuicHint_set_port(second, Cronet_QuicHint_get_port(first));
  EXPECT_EQ(Cronet_QuicHint_get_port(first), Cronet_QuicHint_get_port(second));
  Cronet_QuicHint_set_alternatePort(second,
                                    Cronet_QuicHint_get_alternatePort(first));
  EXPECT_EQ(Cronet_QuicHint_get_alternatePort(first),
            Cronet_QuicHint_get_alternatePort(second));
  Cronet_QuicHint_Destroy(first);
  Cronet_QuicHint_Destroy(second);
}

// Test Struct Cronet_PublicKeyPins setters and getters.
TEST_F(CronetStructTest, TestCronet_PublicKeyPins) {
  Cronet_PublicKeyPinsPtr first = Cronet_PublicKeyPins_Create();
  Cronet_PublicKeyPinsPtr second = Cronet_PublicKeyPins_Create();

  // Copy values from |first| to |second|.
  Cronet_PublicKeyPins_set_host(second, Cronet_PublicKeyPins_get_host(first));
  EXPECT_STREQ(Cronet_PublicKeyPins_get_host(first),
               Cronet_PublicKeyPins_get_host(second));
  // TODO(mef): Test array |pinsSha256|.
  Cronet_PublicKeyPins_set_includeSubdomains(
      second, Cronet_PublicKeyPins_get_includeSubdomains(first));
  EXPECT_EQ(Cronet_PublicKeyPins_get_includeSubdomains(first),
            Cronet_PublicKeyPins_get_includeSubdomains(second));
  Cronet_PublicKeyPins_Destroy(first);
  Cronet_PublicKeyPins_Destroy(second);
}

// Test Struct Cronet_EngineParams setters and getters.
TEST_F(CronetStructTest, TestCronet_EngineParams) {
  Cronet_EngineParamsPtr first = Cronet_EngineParams_Create();
  Cronet_EngineParamsPtr second = Cronet_EngineParams_Create();

  // Copy values from |first| to |second|.
  Cronet_EngineParams_set_userAgent(second,
                                    Cronet_EngineParams_get_userAgent(first));
  EXPECT_STREQ(Cronet_EngineParams_get_userAgent(first),
               Cronet_EngineParams_get_userAgent(second));
  Cronet_EngineParams_set_storagePath(
      second, Cronet_EngineParams_get_storagePath(first));
  EXPECT_STREQ(Cronet_EngineParams_get_storagePath(first),
               Cronet_EngineParams_get_storagePath(second));
  Cronet_EngineParams_set_enableQuic(second,
                                     Cronet_EngineParams_get_enableQuic(first));
  EXPECT_EQ(Cronet_EngineParams_get_enableQuic(first),
            Cronet_EngineParams_get_enableQuic(second));
  Cronet_EngineParams_set_enableHttp2(
      second, Cronet_EngineParams_get_enableHttp2(first));
  EXPECT_EQ(Cronet_EngineParams_get_enableHttp2(first),
            Cronet_EngineParams_get_enableHttp2(second));
  Cronet_EngineParams_set_enableBrotli(
      second, Cronet_EngineParams_get_enableBrotli(first));
  EXPECT_EQ(Cronet_EngineParams_get_enableBrotli(first),
            Cronet_EngineParams_get_enableBrotli(second));
  Cronet_EngineParams_set_httpCacheMode(
      second, Cronet_EngineParams_get_httpCacheMode(first));
  EXPECT_EQ(Cronet_EngineParams_get_httpCacheMode(first),
            Cronet_EngineParams_get_httpCacheMode(second));
  Cronet_EngineParams_set_httpCacheMaxSize(
      second, Cronet_EngineParams_get_httpCacheMaxSize(first));
  EXPECT_EQ(Cronet_EngineParams_get_httpCacheMaxSize(first),
            Cronet_EngineParams_get_httpCacheMaxSize(second));
  // TODO(mef): Test array |quicHints|.
  // TODO(mef): Test array |publicKeyPins|.
  Cronet_EngineParams_set_enablePublicKeyPinningBypassForLocalTrustAnchors(
      second,
      Cronet_EngineParams_get_enablePublicKeyPinningBypassForLocalTrustAnchors(
          first));
  EXPECT_EQ(
      Cronet_EngineParams_get_enablePublicKeyPinningBypassForLocalTrustAnchors(
          first),
      Cronet_EngineParams_get_enablePublicKeyPinningBypassForLocalTrustAnchors(
          second));
  Cronet_EngineParams_Destroy(first);
  Cronet_EngineParams_Destroy(second);
}

// Test Struct Cronet_HttpHeader setters and getters.
TEST_F(CronetStructTest, TestCronet_HttpHeader) {
  Cronet_HttpHeaderPtr first = Cronet_HttpHeader_Create();
  Cronet_HttpHeaderPtr second = Cronet_HttpHeader_Create();

  // Copy values from |first| to |second|.
  Cronet_HttpHeader_set_name(second, Cronet_HttpHeader_get_name(first));
  EXPECT_STREQ(Cronet_HttpHeader_get_name(first),
               Cronet_HttpHeader_get_name(second));
  Cronet_HttpHeader_set_value(second, Cronet_HttpHeader_get_value(first));
  EXPECT_STREQ(Cronet_HttpHeader_get_value(first),
               Cronet_HttpHeader_get_value(second));
  Cronet_HttpHeader_Destroy(first);
  Cronet_HttpHeader_Destroy(second);
}

// Test Struct Cronet_UrlResponseInfo setters and getters.
TEST_F(CronetStructTest, TestCronet_UrlResponseInfo) {
  Cronet_UrlResponseInfoPtr first = Cronet_UrlResponseInfo_Create();
  Cronet_UrlResponseInfoPtr second = Cronet_UrlResponseInfo_Create();

  // Copy values from |first| to |second|.
  Cronet_UrlResponseInfo_set_url(second, Cronet_UrlResponseInfo_get_url(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_get_url(first),
               Cronet_UrlResponseInfo_get_url(second));
  // TODO(mef): Test array |urlChain|.
  Cronet_UrlResponseInfo_set_httpStatusCode(
      second, Cronet_UrlResponseInfo_get_httpStatusCode(first));
  EXPECT_EQ(Cronet_UrlResponseInfo_get_httpStatusCode(first),
            Cronet_UrlResponseInfo_get_httpStatusCode(second));
  Cronet_UrlResponseInfo_set_httpStatusText(
      second, Cronet_UrlResponseInfo_get_httpStatusText(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_get_httpStatusText(first),
               Cronet_UrlResponseInfo_get_httpStatusText(second));
  // TODO(mef): Test array |allHeadersList|.
  Cronet_UrlResponseInfo_set_wasCached(
      second, Cronet_UrlResponseInfo_get_wasCached(first));
  EXPECT_EQ(Cronet_UrlResponseInfo_get_wasCached(first),
            Cronet_UrlResponseInfo_get_wasCached(second));
  Cronet_UrlResponseInfo_set_negotiatedProtocol(
      second, Cronet_UrlResponseInfo_get_negotiatedProtocol(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_get_negotiatedProtocol(first),
               Cronet_UrlResponseInfo_get_negotiatedProtocol(second));
  Cronet_UrlResponseInfo_set_proxyServer(
      second, Cronet_UrlResponseInfo_get_proxyServer(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_get_proxyServer(first),
               Cronet_UrlResponseInfo_get_proxyServer(second));
  Cronet_UrlResponseInfo_set_receivedByteCount(
      second, Cronet_UrlResponseInfo_get_receivedByteCount(first));
  EXPECT_EQ(Cronet_UrlResponseInfo_get_receivedByteCount(first),
            Cronet_UrlResponseInfo_get_receivedByteCount(second));
  Cronet_UrlResponseInfo_Destroy(first);
  Cronet_UrlResponseInfo_Destroy(second);
}

// Test Struct Cronet_UrlRequestParams setters and getters.
TEST_F(CronetStructTest, TestCronet_UrlRequestParams) {
  Cronet_UrlRequestParamsPtr first = Cronet_UrlRequestParams_Create();
  Cronet_UrlRequestParamsPtr second = Cronet_UrlRequestParams_Create();

  // Copy values from |first| to |second|.
  Cronet_UrlRequestParams_set_httpMethod(
      second, Cronet_UrlRequestParams_get_httpMethod(first));
  EXPECT_STREQ(Cronet_UrlRequestParams_get_httpMethod(first),
               Cronet_UrlRequestParams_get_httpMethod(second));
  // TODO(mef): Test array |requestHeaders|.
  Cronet_UrlRequestParams_set_disableCache(
      second, Cronet_UrlRequestParams_get_disableCache(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_disableCache(first),
            Cronet_UrlRequestParams_get_disableCache(second));
  Cronet_UrlRequestParams_set_priority(
      second, Cronet_UrlRequestParams_get_priority(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_priority(first),
            Cronet_UrlRequestParams_get_priority(second));
  Cronet_UrlRequestParams_set_uploadDataProvider(
      second, Cronet_UrlRequestParams_get_uploadDataProvider(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_uploadDataProvider(first),
            Cronet_UrlRequestParams_get_uploadDataProvider(second));
  Cronet_UrlRequestParams_set_uploadDataProviderExecutor(
      second, Cronet_UrlRequestParams_get_uploadDataProviderExecutor(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_uploadDataProviderExecutor(first),
            Cronet_UrlRequestParams_get_uploadDataProviderExecutor(second));
  Cronet_UrlRequestParams_set_allowDirectExecutor(
      second, Cronet_UrlRequestParams_get_allowDirectExecutor(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_allowDirectExecutor(first),
            Cronet_UrlRequestParams_get_allowDirectExecutor(second));
  // TODO(mef): Test array |annotations|.
  Cronet_UrlRequestParams_Destroy(first);
  Cronet_UrlRequestParams_Destroy(second);
}
