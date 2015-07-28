// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ssl_status_serialization.h"

#include "net/ssl/ssl_connection_status_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Test that a valid serialized SSLStatus returns true on
// deserialization and deserializes correctly.
TEST(SSLStatusSerializationTest, DeserializeSerializedStatus) {
  // Serialize dummy data and test that it deserializes properly.
  SSLStatus status;
  status.security_style = SECURITY_STYLE_AUTHENTICATED;
  status.cert_id = 1;
  status.cert_status = net::CERT_STATUS_DATE_INVALID;
  status.security_bits = 80;
  status.connection_status = net::SSL_CONNECTION_VERSION_TLS1_2;
  SignedCertificateTimestampIDAndStatus sct(1, net::ct::SCT_STATUS_OK);
  status.signed_certificate_timestamp_ids.push_back(sct);

  std::string serialized = SerializeSecurityInfo(status);

  SSLStatus deserialized;
  ASSERT_TRUE(DeserializeSecurityInfo(serialized, &deserialized));
  EXPECT_EQ(status.security_style, deserialized.security_style);
  EXPECT_EQ(status.cert_id, deserialized.cert_id);
  EXPECT_EQ(status.cert_status, deserialized.cert_status);
  EXPECT_EQ(status.security_bits, deserialized.security_bits);
  EXPECT_EQ(status.connection_status, deserialized.connection_status);
  EXPECT_EQ(status.signed_certificate_timestamp_ids.size(),
            deserialized.signed_certificate_timestamp_ids.size());
  EXPECT_EQ(sct, deserialized.signed_certificate_timestamp_ids[0]);
  // Test that |content_status| has the default (initialized) value.
  EXPECT_EQ(SSLStatus::NORMAL_CONTENT, deserialized.content_status);
}

// Test that an invalid serialized SSLStatus returns false on
// deserialization.
TEST(SSLStatusSerializationTest, DeserializeBogusStatus) {
  // Test that a failure to deserialize returns false and returns
  // initialized, default data.
  SSLStatus invalid_deserialized;
  ASSERT_FALSE(
      DeserializeSecurityInfo("not an SSLStatus", &invalid_deserialized));

  SSLStatus default_ssl_status;
  EXPECT_EQ(default_ssl_status.security_style,
            invalid_deserialized.security_style);
  EXPECT_EQ(default_ssl_status.cert_id, invalid_deserialized.cert_id);
  EXPECT_EQ(default_ssl_status.cert_status, invalid_deserialized.cert_status);
  EXPECT_EQ(default_ssl_status.security_bits,
            invalid_deserialized.security_bits);
  EXPECT_EQ(default_ssl_status.connection_status,
            invalid_deserialized.connection_status);
  EXPECT_EQ(default_ssl_status.content_status,
            invalid_deserialized.content_status);
  EXPECT_EQ(0u, invalid_deserialized.signed_certificate_timestamp_ids.size());

  // Serialize a status with a bad |security_bits| value and test that
  // deserializing it fails.
  SSLStatus status;
  status.security_style = SECURITY_STYLE_AUTHENTICATED;
  status.cert_id = 1;
  status.cert_status = net::CERT_STATUS_DATE_INVALID;
  // |security_bits| must be <-1. (-1 means the strength is unknown, and
  // |0 means the connection is not encrypted).
  status.security_bits = -5;
  status.connection_status = net::SSL_CONNECTION_VERSION_TLS1_2;
  SignedCertificateTimestampIDAndStatus sct(1, net::ct::SCT_STATUS_OK);
  status.signed_certificate_timestamp_ids.push_back(sct);

  std::string serialized = SerializeSecurityInfo(status);
  ASSERT_FALSE(DeserializeSecurityInfo(serialized, &invalid_deserialized));

  EXPECT_EQ(default_ssl_status.security_style,
            invalid_deserialized.security_style);
  EXPECT_EQ(default_ssl_status.cert_id, invalid_deserialized.cert_id);
  EXPECT_EQ(default_ssl_status.cert_status, invalid_deserialized.cert_status);
  EXPECT_EQ(default_ssl_status.security_bits,
            invalid_deserialized.security_bits);
  EXPECT_EQ(default_ssl_status.connection_status,
            invalid_deserialized.connection_status);
  EXPECT_EQ(default_ssl_status.content_status,
            invalid_deserialized.content_status);
  EXPECT_EQ(0u, invalid_deserialized.signed_certificate_timestamp_ids.size());

  // Now serialize a status with a bad |security_style| value and test
  // that deserializing fails.
  status.security_bits = 128;
  status.security_style = static_cast<SecurityStyle>(100);
  serialized = SerializeSecurityInfo(status);
  ASSERT_FALSE(DeserializeSecurityInfo(serialized, &invalid_deserialized));

  EXPECT_EQ(default_ssl_status.security_style,
            invalid_deserialized.security_style);
  EXPECT_EQ(default_ssl_status.cert_id, invalid_deserialized.cert_id);
  EXPECT_EQ(default_ssl_status.cert_status, invalid_deserialized.cert_status);
  EXPECT_EQ(default_ssl_status.security_bits,
            invalid_deserialized.security_bits);
  EXPECT_EQ(default_ssl_status.connection_status,
            invalid_deserialized.connection_status);
  EXPECT_EQ(default_ssl_status.content_status,
            invalid_deserialized.content_status);
  EXPECT_EQ(0u, invalid_deserialized.signed_certificate_timestamp_ids.size());
}

}  // namespace
