// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/encryption_header_parsers.h"

#include <vector>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const uint64_t kDefaultRecordSize = 4096;

TEST(EncryptionHeaderParsersTest, ParseValidEncryptionHeaders) {
  struct {
    const char* const header;
    const char* const parsed_keyid;
    const char* const parsed_salt;
    uint64_t parsed_rs;
  } expected_results[] = {
    { "keyid=foo;salt=c2l4dGVlbmNvb2xieXRlcw;rs=1024",
      "foo", "sixteencoolbytes", 1024 },
    { "keyid=foo; salt=c2l4dGVlbmNvb2xieXRlcw; rs=1024",
      "foo", "sixteencoolbytes", 1024 },
    { "KEYID=foo;SALT=c2l4dGVlbmNvb2xieXRlcw;RS=1024",
      "foo", "sixteencoolbytes", 1024 },
    { " keyid = foo ; salt = c2l4dGVlbmNvb2xieXRlcw ; rs = 1024 ",
      "foo", "sixteencoolbytes", 1024 },
    { "keyid=foo", "foo", "", kDefaultRecordSize },
    { "keyid=foo;", "foo", "", kDefaultRecordSize },
    { "keyid=\"foo\"", "foo", "", kDefaultRecordSize },
    { "keyid='foo'", "foo", "", kDefaultRecordSize },
    { "salt=c2l4dGVlbmNvb2xieXRlcw",
      "", "sixteencoolbytes", kDefaultRecordSize },
    { "rs=2048", "", "", 2048 },
    { "keyid=foo;someothervalue=1;rs=42", "foo", "", 42 },
    { "keyid=foo;keyid=bar", "bar", "", kDefaultRecordSize },
  };

  for (size_t i = 0; i < arraysize(expected_results); i++) {
    SCOPED_TRACE(i);

    std::vector<EncryptionHeaderValues> values;
    ASSERT_TRUE(ParseEncryptionHeader(expected_results[i].header, &values));
    ASSERT_EQ(1u, values.size());

    EXPECT_EQ(expected_results[i].parsed_keyid, values[0].keyid);
    EXPECT_EQ(expected_results[i].parsed_salt, values[0].salt);
    EXPECT_EQ(expected_results[i].parsed_rs, values[0].rs);
  }
}

TEST(EncryptionHeaderParsersTest, ParseValidMultiValueEncryptionHeaders) {
  const size_t kNumberOfValues = 2u;

  struct {
    const char* const header;
    struct {
      const char* const keyid;
      const char* const salt;
      uint64_t rs;
    } parsed_values[kNumberOfValues];
  } expected_results[] = {
    { "keyid=foo;salt=c2l4dGVlbmNvb2xieXRlcw;rs=1024,keyid=foo;salt=c2l4dGVlbm"
          "Nvb2xieXRlcw;rs=1024",
      { { "foo", "sixteencoolbytes", 1024 },
        { "foo", "sixteencoolbytes", 1024 } } },
    { "keyid=foo,salt=c2l4dGVlbmNvb2xieXRlcw;rs=1024",
      { { "foo", "", kDefaultRecordSize },
        { "", "sixteencoolbytes", 1024 } } },
    { "keyid=foo,keyid=bar;salt=c2l4dGVlbmNvb2xieXRlcw;rs=1024",
      { { "foo", "", kDefaultRecordSize },
        { "bar", "sixteencoolbytes", 1024 } } },
    { "keyid=\"foo,keyid=bar\",salt=c2l4dGVlbmNvb2xieXRlcw",
      { { "foo,keyid=bar", "", kDefaultRecordSize },
        { "", "sixteencoolbytes", kDefaultRecordSize } } },
  };

  for (size_t i = 0; i < arraysize(expected_results); i++) {
    SCOPED_TRACE(i);

    std::vector<EncryptionHeaderValues> values;
    ASSERT_TRUE(ParseEncryptionHeader(expected_results[i].header, &values));
    ASSERT_EQ(kNumberOfValues, values.size());

    for (size_t j = 0; j < kNumberOfValues; ++j) {
      EXPECT_EQ(expected_results[i].parsed_values[j].keyid, values[j].keyid);
      EXPECT_EQ(expected_results[i].parsed_values[j].salt, values[j].salt);
      EXPECT_EQ(expected_results[i].parsed_values[j].rs, values[j].rs);
    }
  }
}

TEST(EncryptionHeaderParsersTest, ParseInvalidEncryptionHeaders) {
  const char* const expected_failures[] = {
    "keyid",
    "keyid=",
    "keyid=foo;novaluekey",
    "keyid=foo,keyid",
    "salt",
    "salt=",
    "salt=YmV/2ZXJ-sMDA",
    "salt=dHdlbHZlY29vbGJ5dGVz=====",
    "salt=123$xyz",
    "salt=c2l4dGVlbmNvb2xieXRlcw,salt=123$xyz",
    "rs",
    "rs=",
    "rs=0",
    "rs=0x13",
    "rs=1",
    "rs=-1",
    "rs=+5",
    "rs=99999999999999999999999999999999",
    "rs=2,rs=0",
    "rs=foobar",
  };

  for (size_t i = 0; i < arraysize(expected_failures); i++) {
    SCOPED_TRACE(i);

    std::vector<EncryptionHeaderValues> values;
    EXPECT_FALSE(ParseEncryptionHeader(expected_failures[i], &values));
    EXPECT_EQ(0u, values.size());
  }
}

TEST(EncryptionHeaderParsersTest, ParseValidCryptoKeyHeaders) {
  struct {
    const char* const header;
    const char* const parsed_keyid;
    const char* const parsed_aesgcm128;
    const char* const parsed_dh;
  } expected_results[] = {
    { "keyid=foo;aesgcm128=c2l4dGVlbmNvb2xieXRlcw;dh=dHdlbHZlY29vbGJ5dGVz",
      "foo", "sixteencoolbytes", "twelvecoolbytes" },
    { "keyid=foo; aesgcm128=c2l4dGVlbmNvb2xieXRlcw; dh=dHdlbHZlY29vbGJ5dGVz",
      "foo", "sixteencoolbytes", "twelvecoolbytes" },
    { "keyid = foo ; aesgcm128 = c2l4dGVlbmNvb2xieXRlcw ; dh = dHdlbHZlY29vbGJ5"
          "dGVz ",
      "foo", "sixteencoolbytes", "twelvecoolbytes" },
    { "KEYID=foo;AESGCM128=c2l4dGVlbmNvb2xieXRlcw;DH=dHdlbHZlY29vbGJ5dGVz",
      "foo", "sixteencoolbytes", "twelvecoolbytes" },
    { "keyid=foo", "foo", "", "" },
    { "aesgcm128=c2l4dGVlbmNvb2xieXRlcw", "", "sixteencoolbytes", "" },
    { "aesgcm128=\"c2l4dGVlbmNvb2xieXRlcw\"", "", "sixteencoolbytes", "" },
    { "aesgcm128='c2l4dGVlbmNvb2xieXRlcw'", "", "sixteencoolbytes", "" },
    { "dh=dHdlbHZlY29vbGJ5dGVz", "", "", "twelvecoolbytes" },
    { "keyid=foo;someothervalue=bar;aesgcm128=dHdlbHZlY29vbGJ5dGVz",
      "foo", "twelvecoolbytes", "" },
    { "keyid=foo;keyid=bar", "bar", "", "" },
  };

  for (size_t i = 0; i < arraysize(expected_results); i++) {
    SCOPED_TRACE(i);

    std::vector<CryptoKeyHeaderValues> values;
    ASSERT_TRUE(ParseCryptoKeyHeader(expected_results[i].header, &values));
    ASSERT_EQ(1u, values.size());

    EXPECT_EQ(expected_results[i].parsed_keyid, values[0].keyid);
    EXPECT_EQ(expected_results[i].parsed_aesgcm128, values[0].aesgcm128);
    EXPECT_EQ(expected_results[i].parsed_dh, values[0].dh);
  }
}

TEST(EncryptionHeaderParsersTest, ParseValidMultiValueCryptoKeyHeaders) {
  const size_t kNumberOfValues = 2u;

  struct {
    const char* const header;
    struct {
      const char* const keyid;
      const char* const aesgcm128;
      const char* const dh;
    } parsed_values[kNumberOfValues];
  } expected_results[] = {
    { "keyid=foo;aesgcm128=c2l4dGVlbmNvb2xieXRlcw;dh=dHdlbHZlY29vbGJ5dGVz,"
          "keyid=bar;aesgcm128=dHdlbHZlY29vbGJ5dGVz;dh=c2l4dGVlbmNvb2xieXRlcw",
      { { "foo", "sixteencoolbytes", "twelvecoolbytes" },
        { "bar", "twelvecoolbytes", "sixteencoolbytes" } } },
    { "keyid=foo,aesgcm128=c2l4dGVlbmNvb2xieXRlcw",
      { { "foo", "", "" },
        { "", "sixteencoolbytes", "" } } },
    { "keyid=foo,keyid=bar;dh=dHdlbHZlY29vbGJ5dGVz",
      { { "foo", "", "" },
        { "bar", "", "twelvecoolbytes" } } },
    { "keyid=\"foo,keyid=bar\",aesgcm128=c2l4dGVlbmNvb2xieXRlcw",
      { { "foo,keyid=bar", "", "" },
        { "", "sixteencoolbytes", "" } } },
  };

  for (size_t i = 0; i < arraysize(expected_results); i++) {
    SCOPED_TRACE(i);

    std::vector<CryptoKeyHeaderValues> values;
    ASSERT_TRUE(ParseCryptoKeyHeader(expected_results[i].header, &values));
    ASSERT_EQ(kNumberOfValues, values.size());

    for (size_t j = 0; j < kNumberOfValues; ++j) {
      EXPECT_EQ(expected_results[i].parsed_values[j].keyid, values[j].keyid);
      EXPECT_EQ(expected_results[i].parsed_values[j].aesgcm128,
                values[j].aesgcm128);
      EXPECT_EQ(expected_results[i].parsed_values[j].dh, values[j].dh);
    }
  }
}

TEST(EncryptionHeaderParsersTest, ParseInvalidCryptoKeyHeaders) {
  const char* const expected_failures[] = {
    "keyid",
    "keyid=",
    "keyid=foo,keyid",
    "keyid=foo;novaluekey",
    "aesgcm128",
    "aesgcm128=",
    "aesgcm128=123$xyz",
    "aesgcm128=foobar,aesgcm128=123$xyz",
    "dh",
    "dh=",
    "dh=YmV/2ZXJ-sMDA",
    "dh=dHdlbHZlY29vbGJ5dGVz=====",
    "dh=123$xyz",
  };

  for (size_t i = 0; i < arraysize(expected_failures); i++) {
    SCOPED_TRACE(i);

    std::vector<CryptoKeyHeaderValues> values;
    EXPECT_FALSE(ParseCryptoKeyHeader(expected_failures[i], &values));
    EXPECT_EQ(0u, values.size());
  }
}

TEST(EncryptionHeaderParsersTest, SixValueHeader) {
  const char* const header = "keyid=0,keyid=1,keyid=2,keyid=3,keyid=4,keyid=5";

  std::vector<EncryptionHeaderValues> encryption_values;
  ASSERT_TRUE(ParseEncryptionHeader(header, &encryption_values));

  std::vector<CryptoKeyHeaderValues> crypto_key_values;
  ASSERT_TRUE(ParseCryptoKeyHeader(header, &crypto_key_values));

  ASSERT_EQ(6u, encryption_values.size());
  ASSERT_EQ(6u, crypto_key_values.size());

  for (size_t i = 0; i < encryption_values.size(); i++) {
    SCOPED_TRACE(i);

    const std::string value = base::IntToString(i);

    EXPECT_EQ(value, encryption_values[i].keyid);
    EXPECT_EQ(value, crypto_key_values[i].keyid);
  }
}

TEST(EncryptionHeaderParsersTest, InvalidHeadersDoNotModifyOutput) {
  EncryptionHeaderValues encryption_value;
  encryption_value.keyid = "mykeyid";
  encryption_value.salt = "somesalt";
  encryption_value.rs = 42u;

  std::vector<EncryptionHeaderValues> encryption_values;
  encryption_values.push_back(encryption_value);

  ASSERT_FALSE(ParseEncryptionHeader("rs=foobar", &encryption_values));
  ASSERT_EQ(1u, encryption_values.size());

  EXPECT_EQ("mykeyid", encryption_values[0].keyid);
  EXPECT_EQ("somesalt", encryption_values[0].salt);
  EXPECT_EQ(42u, encryption_values[0].rs);

  CryptoKeyHeaderValues crypto_key_value;
  crypto_key_value.keyid = "myotherkeyid";
  crypto_key_value.aesgcm128 = "akey";
  crypto_key_value.dh = "yourdh";

  std::vector<CryptoKeyHeaderValues> crypto_key_values;
  crypto_key_values.push_back(crypto_key_value);

  ASSERT_FALSE(ParseCryptoKeyHeader("aesgcm128=$$$", &crypto_key_values));
  ASSERT_EQ(1u, crypto_key_values.size());

  EXPECT_EQ("myotherkeyid", crypto_key_values[0].keyid);
  EXPECT_EQ("akey", crypto_key_values[0].aesgcm128);
  EXPECT_EQ("yourdh", crypto_key_values[0].dh);
}

}  // namespace

}  // namespace gcm
