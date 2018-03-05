// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_header.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "content/public/common/content_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(SignedExchangeHeaderTest, ParseHeaderLength) {
  constexpr struct {
    uint8_t bytes[SignedExchangeHeader::kEncodedHeaderLengthInBytes];
    size_t expected;
  } kTestCases[] = {
      {{0x00, 0x00, 0x01}, 1u}, {{0x01, 0xe2, 0x40}, 123456u},
  };

  int test_element_index = 0;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "testing case " << test_element_index++);
    EXPECT_EQ(SignedExchangeHeader::ParseHeadersLength(test_case.bytes),
              test_case.expected);
  }
}

TEST(SignedExchangeHeaderTest, Parse) {
  base::FilePath test_htxg_path;
  PathService::Get(content::DIR_TEST_DATA, &test_htxg_path);
  test_htxg_path = test_htxg_path.AppendASCII("htxg").AppendASCII(
      "signed_exchange_header_test.htxg");

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_htxg_path, &contents));
  auto* contents_bytes = reinterpret_cast<const uint8_t*>(contents.data());

  ASSERT_GT(contents.size(), SignedExchangeHeader::kEncodedHeaderLengthInBytes);
  size_t header_size = SignedExchangeHeader::ParseHeadersLength(base::make_span(
      contents_bytes, SignedExchangeHeader::kEncodedHeaderLengthInBytes));
  ASSERT_GT(contents.size(),
            SignedExchangeHeader::kEncodedHeaderLengthInBytes + header_size);

  const auto cbor_bytes = base::make_span<const uint8_t>(
      contents_bytes + SignedExchangeHeader::kEncodedHeaderLengthInBytes,
      header_size);
  const base::Optional<SignedExchangeHeader> header =
      SignedExchangeHeader::Parse(cbor_bytes);
  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(header->request_url(), GURL("https://test.example.org/test/"));
  EXPECT_EQ(header->request_method(), "GET");
  EXPECT_EQ(header->response_code(), static_cast<net::HttpStatusCode>(200u));
  EXPECT_EQ(header->response_headers().size(), 5u);
  EXPECT_EQ(header->response_headers().find("content-encoding")->second,
            "mi-sha256");
}

}  // namespace content
