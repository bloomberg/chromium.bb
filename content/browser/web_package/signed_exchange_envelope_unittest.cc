// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_envelope.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/cbor/cbor_values.h"
#include "components/cbor/cbor_writer.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_prologue.h"
#include "content/public/common/content_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kSignatureString[] =
    "sig1;"
    " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
    "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
    " integrity=\"mi\";"
    " validity-url=\"https://example.com/resource.validity.1511128380\";"
    " cert-url=\"https://example.com/oldcerts\";"
    " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
    " date=1511128380; expires=1511733180";

cbor::CBORValue CBORByteString(const char* str) {
  return cbor::CBORValue(str, cbor::CBORValue::Type::BYTE_STRING);
}

base::Optional<SignedExchangeEnvelope> GenerateHeaderAndParse(
    const std::map<const char*, const char*>& request_map,
    const std::map<const char*, const char*>& response_map) {
  cbor::CBORValue::MapValue request_cbor_map;
  cbor::CBORValue::MapValue response_cbor_map;
  for (auto& pair : request_map)
    request_cbor_map[CBORByteString(pair.first)] = CBORByteString(pair.second);
  for (auto& pair : response_map)
    response_cbor_map[CBORByteString(pair.first)] = CBORByteString(pair.second);

  cbor::CBORValue::ArrayValue array;
  array.push_back(cbor::CBORValue(std::move(request_cbor_map)));
  array.push_back(cbor::CBORValue(std::move(response_cbor_map)));

  auto serialized = cbor::CBORWriter::Write(cbor::CBORValue(std::move(array)));
  return SignedExchangeEnvelope::Parse(
      base::make_span(serialized->data(), serialized->size()),
      nullptr /* devtools_proxy */);
}

}  // namespace

TEST(SignedExchangeEnvelopeTest, ParseGoldenFile) {
  base::FilePath test_htxg_path;
  base::PathService::Get(content::DIR_TEST_DATA, &test_htxg_path);
  test_htxg_path = test_htxg_path.AppendASCII("htxg").AppendASCII(
      "test.example.org_test.htxg");

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_htxg_path, &contents));
  auto* contents_bytes = reinterpret_cast<const uint8_t*>(contents.data());

  ASSERT_GT(contents.size(), SignedExchangePrologue::kEncodedLengthInBytes);
  size_t header_size =
      SignedExchangePrologue::ParseEncodedLength(base::make_span(
          contents_bytes, SignedExchangePrologue::kEncodedLengthInBytes));
  ASSERT_GT(contents.size(),
            SignedExchangePrologue::kEncodedLengthInBytes + header_size);

  const auto cbor_bytes = base::make_span<const uint8_t>(
      contents_bytes + SignedExchangePrologue::kEncodedLengthInBytes,
      header_size);
  const base::Optional<SignedExchangeEnvelope> header =
      SignedExchangeEnvelope::Parse(cbor_bytes, nullptr /* devtools_proxy */);
  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(header->request_url(), GURL("https://test.example.org/test/"));
  EXPECT_EQ(header->request_method(), "GET");
  EXPECT_EQ(header->response_code(), static_cast<net::HttpStatusCode>(200u));
  EXPECT_EQ(header->response_headers().size(), 4u);
  EXPECT_EQ(header->response_headers().find("content-encoding")->second,
            "mi-sha256");
}

TEST(SignedExchangeEnvelopeTest, ValidHeader) {
  auto header = GenerateHeaderAndParse(
      {
          {kUrlKey, "https://test.example.org/test/"}, {kMethodKey, "GET"},
      },
      {
          {kStatusKey, "200"}, {kSignature, kSignatureString},
      });
  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(header->request_url(), GURL("https://test.example.org/test/"));
  EXPECT_EQ(header->request_method(), "GET");
  EXPECT_EQ(header->response_code(), static_cast<net::HttpStatusCode>(200u));
  EXPECT_EQ(header->response_headers().size(), 1u);
}

TEST(SignedExchangeEnvelopeTest, UnsafeMethod) {
  auto header = GenerateHeaderAndParse(
      {
          {kUrlKey, "https://test.example.org/test/"}, {kMethodKey, "POST"},
      },
      {
          {kStatusKey, "200"}, {kSignature, kSignatureString},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, InvalidURL) {
  auto header = GenerateHeaderAndParse(
      {
          {kUrlKey, "https:://test.example.org/test/"}, {kMethodKey, "GET"},
      },
      {
          {kStatusKey, "200"}, {kSignature, kSignatureString},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, URLWithFragment) {
  auto header = GenerateHeaderAndParse(
      {
          {kUrlKey, "https://test.example.org/test/#foo"}, {kMethodKey, "GET"},
      },
      {
          {kStatusKey, "200"}, {kSignature, kSignatureString},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, RelativeURL) {
  auto header = GenerateHeaderAndParse(
      {
          {kUrlKey, "test/"}, {kMethodKey, "GET"},
      },
      {
          {kStatusKey, "200"}, {kSignature, kSignatureString},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, StatefulRequestHeader) {
  auto header = GenerateHeaderAndParse(
      {
          {kUrlKey, "https://test.example.org/test/"},
          {kMethodKey, "GET"},
          {"authorization", "Basic Zm9vOmJhcg=="},
      },
      {
          {kStatusKey, "200"}, {kSignature, kSignatureString},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, StatefulResponseHeader) {
  auto header = GenerateHeaderAndParse(
      {
          {kUrlKey, "https://test.example.org/test/"}, {kMethodKey, "GET"},
      },
      {
          {kStatusKey, "200"},
          {kSignature, kSignatureString},
          {"set-cookie", "foo=bar"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, UppercaseRequestMap) {
  auto header = GenerateHeaderAndParse(
      {{kUrlKey, "https://test.example.org/test/"},
       {kMethodKey, "GET"},
       {"Accept-Language", "en-us"}},
      {
          {kStatusKey, "200"}, {kSignature, kSignatureString},
      });
  ASSERT_FALSE(header.has_value());
}

TEST(SignedExchangeEnvelopeTest, UppercaseResponseMap) {
  auto header = GenerateHeaderAndParse(
      {
          {kUrlKey, "https://test.example.org/test/"}, {kMethodKey, "GET"},
      },
      {{kStatusKey, "200"},
       {kSignature, kSignatureString},
       {"Content-Length", "123"}});
  ASSERT_FALSE(header.has_value());
}

}  // namespace content
