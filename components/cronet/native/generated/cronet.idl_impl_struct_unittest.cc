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

// Test Struct Cronet_Error setters and getters.
TEST_F(CronetStructTest, TestCronet_Error) {
  Cronet_ErrorPtr first = Cronet_Error_Create();
  Cronet_ErrorPtr second = Cronet_Error_Create();

  // Copy values from |first| to |second|.
  Cronet_Error_set_errorCode(second, Cronet_Error_get_errorCode(first));
  EXPECT_EQ(Cronet_Error_get_errorCode(first),
            Cronet_Error_get_errorCode(second));
  Cronet_Error_set_message(second, Cronet_Error_get_message(first));
  EXPECT_STREQ(Cronet_Error_get_message(first),
               Cronet_Error_get_message(second));
  Cronet_Error_set_internal_error_code(
      second, Cronet_Error_get_internal_error_code(first));
  EXPECT_EQ(Cronet_Error_get_internal_error_code(first),
            Cronet_Error_get_internal_error_code(second));
  Cronet_Error_set_immediately_retryable(
      second, Cronet_Error_get_immediately_retryable(first));
  EXPECT_EQ(Cronet_Error_get_immediately_retryable(first),
            Cronet_Error_get_immediately_retryable(second));
  Cronet_Error_set_quic_detailed_error_code(
      second, Cronet_Error_get_quic_detailed_error_code(first));
  EXPECT_EQ(Cronet_Error_get_quic_detailed_error_code(first),
            Cronet_Error_get_quic_detailed_error_code(second));
  Cronet_Error_Destroy(first);
  Cronet_Error_Destroy(second);
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
  Cronet_QuicHint_set_alternate_port(second,
                                     Cronet_QuicHint_get_alternate_port(first));
  EXPECT_EQ(Cronet_QuicHint_get_alternate_port(first),
            Cronet_QuicHint_get_alternate_port(second));
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
  // TODO(mef): Test array |pins_sha256|.
  Cronet_PublicKeyPins_set_include_subdomains(
      second, Cronet_PublicKeyPins_get_include_subdomains(first));
  EXPECT_EQ(Cronet_PublicKeyPins_get_include_subdomains(first),
            Cronet_PublicKeyPins_get_include_subdomains(second));
  Cronet_PublicKeyPins_set_expiration_date(
      second, Cronet_PublicKeyPins_get_expiration_date(first));
  EXPECT_EQ(Cronet_PublicKeyPins_get_expiration_date(first),
            Cronet_PublicKeyPins_get_expiration_date(second));
  Cronet_PublicKeyPins_Destroy(first);
  Cronet_PublicKeyPins_Destroy(second);
}

// Test Struct Cronet_EngineParams setters and getters.
TEST_F(CronetStructTest, TestCronet_EngineParams) {
  Cronet_EngineParamsPtr first = Cronet_EngineParams_Create();
  Cronet_EngineParamsPtr second = Cronet_EngineParams_Create();

  // Copy values from |first| to |second|.
  Cronet_EngineParams_set_enable_check_result(
      second, Cronet_EngineParams_get_enable_check_result(first));
  EXPECT_EQ(Cronet_EngineParams_get_enable_check_result(first),
            Cronet_EngineParams_get_enable_check_result(second));
  Cronet_EngineParams_set_user_agent(second,
                                     Cronet_EngineParams_get_user_agent(first));
  EXPECT_STREQ(Cronet_EngineParams_get_user_agent(first),
               Cronet_EngineParams_get_user_agent(second));
  Cronet_EngineParams_set_accept_language(
      second, Cronet_EngineParams_get_accept_language(first));
  EXPECT_STREQ(Cronet_EngineParams_get_accept_language(first),
               Cronet_EngineParams_get_accept_language(second));
  Cronet_EngineParams_set_storage_path(
      second, Cronet_EngineParams_get_storage_path(first));
  EXPECT_STREQ(Cronet_EngineParams_get_storage_path(first),
               Cronet_EngineParams_get_storage_path(second));
  Cronet_EngineParams_set_enable_quic(
      second, Cronet_EngineParams_get_enable_quic(first));
  EXPECT_EQ(Cronet_EngineParams_get_enable_quic(first),
            Cronet_EngineParams_get_enable_quic(second));
  Cronet_EngineParams_set_enable_http2(
      second, Cronet_EngineParams_get_enable_http2(first));
  EXPECT_EQ(Cronet_EngineParams_get_enable_http2(first),
            Cronet_EngineParams_get_enable_http2(second));
  Cronet_EngineParams_set_enable_brotli(
      second, Cronet_EngineParams_get_enable_brotli(first));
  EXPECT_EQ(Cronet_EngineParams_get_enable_brotli(first),
            Cronet_EngineParams_get_enable_brotli(second));
  Cronet_EngineParams_set_http_cache_mode(
      second, Cronet_EngineParams_get_http_cache_mode(first));
  EXPECT_EQ(Cronet_EngineParams_get_http_cache_mode(first),
            Cronet_EngineParams_get_http_cache_mode(second));
  Cronet_EngineParams_set_http_cache_max_size(
      second, Cronet_EngineParams_get_http_cache_max_size(first));
  EXPECT_EQ(Cronet_EngineParams_get_http_cache_max_size(first),
            Cronet_EngineParams_get_http_cache_max_size(second));
  // TODO(mef): Test array |quic_hints|.
  // TODO(mef): Test array |public_key_pins|.
  Cronet_EngineParams_set_enable_public_key_pinning_bypass_for_local_trust_anchors(
      second,
      Cronet_EngineParams_get_enable_public_key_pinning_bypass_for_local_trust_anchors(
          first));
  EXPECT_EQ(
      Cronet_EngineParams_get_enable_public_key_pinning_bypass_for_local_trust_anchors(
          first),
      Cronet_EngineParams_get_enable_public_key_pinning_bypass_for_local_trust_anchors(
          second));
  Cronet_EngineParams_set_experimental_options(
      second, Cronet_EngineParams_get_experimental_options(first));
  EXPECT_STREQ(Cronet_EngineParams_get_experimental_options(first),
               Cronet_EngineParams_get_experimental_options(second));
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
  // TODO(mef): Test array |url_chain|.
  Cronet_UrlResponseInfo_set_http_status_code(
      second, Cronet_UrlResponseInfo_get_http_status_code(first));
  EXPECT_EQ(Cronet_UrlResponseInfo_get_http_status_code(first),
            Cronet_UrlResponseInfo_get_http_status_code(second));
  Cronet_UrlResponseInfo_set_http_status_text(
      second, Cronet_UrlResponseInfo_get_http_status_text(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_get_http_status_text(first),
               Cronet_UrlResponseInfo_get_http_status_text(second));
  // TODO(mef): Test array |all_headers_list|.
  Cronet_UrlResponseInfo_set_was_cached(
      second, Cronet_UrlResponseInfo_get_was_cached(first));
  EXPECT_EQ(Cronet_UrlResponseInfo_get_was_cached(first),
            Cronet_UrlResponseInfo_get_was_cached(second));
  Cronet_UrlResponseInfo_set_negotiated_protocol(
      second, Cronet_UrlResponseInfo_get_negotiated_protocol(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_get_negotiated_protocol(first),
               Cronet_UrlResponseInfo_get_negotiated_protocol(second));
  Cronet_UrlResponseInfo_set_proxy_server(
      second, Cronet_UrlResponseInfo_get_proxy_server(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_get_proxy_server(first),
               Cronet_UrlResponseInfo_get_proxy_server(second));
  Cronet_UrlResponseInfo_set_received_byte_count(
      second, Cronet_UrlResponseInfo_get_received_byte_count(first));
  EXPECT_EQ(Cronet_UrlResponseInfo_get_received_byte_count(first),
            Cronet_UrlResponseInfo_get_received_byte_count(second));
  Cronet_UrlResponseInfo_Destroy(first);
  Cronet_UrlResponseInfo_Destroy(second);
}

// Test Struct Cronet_UrlRequestParams setters and getters.
TEST_F(CronetStructTest, TestCronet_UrlRequestParams) {
  Cronet_UrlRequestParamsPtr first = Cronet_UrlRequestParams_Create();
  Cronet_UrlRequestParamsPtr second = Cronet_UrlRequestParams_Create();

  // Copy values from |first| to |second|.
  Cronet_UrlRequestParams_set_http_method(
      second, Cronet_UrlRequestParams_get_http_method(first));
  EXPECT_STREQ(Cronet_UrlRequestParams_get_http_method(first),
               Cronet_UrlRequestParams_get_http_method(second));
  // TODO(mef): Test array |request_headers|.
  Cronet_UrlRequestParams_set_disable_cache(
      second, Cronet_UrlRequestParams_get_disable_cache(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_disable_cache(first),
            Cronet_UrlRequestParams_get_disable_cache(second));
  Cronet_UrlRequestParams_set_priority(
      second, Cronet_UrlRequestParams_get_priority(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_priority(first),
            Cronet_UrlRequestParams_get_priority(second));
  Cronet_UrlRequestParams_set_upload_data_provider(
      second, Cronet_UrlRequestParams_get_upload_data_provider(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_upload_data_provider(first),
            Cronet_UrlRequestParams_get_upload_data_provider(second));
  Cronet_UrlRequestParams_set_upload_data_provider_executor(
      second, Cronet_UrlRequestParams_get_upload_data_provider_executor(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_upload_data_provider_executor(first),
            Cronet_UrlRequestParams_get_upload_data_provider_executor(second));
  Cronet_UrlRequestParams_set_allow_direct_executor(
      second, Cronet_UrlRequestParams_get_allow_direct_executor(first));
  EXPECT_EQ(Cronet_UrlRequestParams_get_allow_direct_executor(first),
            Cronet_UrlRequestParams_get_allow_direct_executor(second));
  // TODO(mef): Test array |annotations|.
  Cronet_UrlRequestParams_Destroy(first);
  Cronet_UrlRequestParams_Destroy(second);
}

// Test Struct Cronet_RequestFinishedInfo setters and getters.
TEST_F(CronetStructTest, TestCronet_RequestFinishedInfo) {
  Cronet_RequestFinishedInfoPtr first = Cronet_RequestFinishedInfo_Create();
  Cronet_RequestFinishedInfoPtr second = Cronet_RequestFinishedInfo_Create();

  // Copy values from |first| to |second|.
  Cronet_RequestFinishedInfo_Destroy(first);
  Cronet_RequestFinishedInfo_Destroy(second);
}
