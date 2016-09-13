// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_info.h"

#include "base/pickle.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/http_response_headers.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class HttpResponseInfoTest : public testing::Test {
 protected:
  void SetUp() override {
    response_info_.headers = new HttpResponseHeaders("");
  }

  void PickleAndRestore(const HttpResponseInfo& response_info,
                        HttpResponseInfo* restored_response_info) const {
    base::Pickle pickle;
    response_info.Persist(&pickle, false, false);
    bool truncated = false;
    restored_response_info->InitFromPickle(pickle, &truncated);
  }

  HttpResponseInfo response_info_;
};

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchDefault) {
  EXPECT_FALSE(response_info_.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchCopy) {
  response_info_.unused_since_prefetch = true;
  HttpResponseInfo response_info_clone(response_info_);
  EXPECT_TRUE(response_info_clone.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchPersistFalse) {
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchPersistTrue) {
  response_info_.unused_since_prefetch = true;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, PKPBypassPersistTrue) {
  response_info_.ssl_info.pkp_bypassed = true;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.ssl_info.pkp_bypassed);
}

TEST_F(HttpResponseInfoTest, PKPBypassPersistFalse) {
  response_info_.ssl_info.pkp_bypassed = false;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.ssl_info.pkp_bypassed);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequiredDefault) {
  EXPECT_FALSE(response_info_.async_revalidation_required);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequiredCopy) {
  response_info_.async_revalidation_required = true;
  net::HttpResponseInfo response_info_clone(response_info_);
  EXPECT_TRUE(response_info_clone.async_revalidation_required);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequiredAssign) {
  response_info_.async_revalidation_required = true;
  net::HttpResponseInfo response_info_clone;
  response_info_clone = response_info_;
  EXPECT_TRUE(response_info_clone.async_revalidation_required);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequiredNotPersisted) {
  response_info_.async_revalidation_required = true;
  net::HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.async_revalidation_required);
}

TEST_F(HttpResponseInfoTest, FailsInitFromPickleWithInvalidSCTStatus) {
  // A valid certificate is needed for ssl_info.is_valid() to be true
  // so that the SCTs would be serialized.
  const std::string der_test_cert(net::ct::GetDerEncodedX509Cert());
  response_info_.ssl_info.cert = net::X509Certificate::CreateFromBytes(
      der_test_cert.data(), der_test_cert.length());

  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ct::GetX509CertSCT(&sct);

  response_info_.ssl_info.signed_certificate_timestamps.push_back(
      SignedCertificateTimestampAndStatus(
          sct, ct::SCTVerifyStatus::SCT_STATUS_LOG_UNKNOWN));

  base::Pickle pickle;
  response_info_.Persist(&pickle, false, false);
  bool truncated = false;
  net::HttpResponseInfo restored_response_info;
  EXPECT_TRUE(restored_response_info.InitFromPickle(pickle, &truncated));

  response_info_.ssl_info.signed_certificate_timestamps.push_back(
      SignedCertificateTimestampAndStatus(sct,
                                          static_cast<ct::SCTVerifyStatus>(2)));
  base::Pickle pickle_invalid;
  response_info_.Persist(&pickle_invalid, false, false);
  net::HttpResponseInfo restored_invalid_response;
  EXPECT_FALSE(
      restored_invalid_response.InitFromPickle(pickle_invalid, &truncated));
}

}  // namespace

}  // namespace net
