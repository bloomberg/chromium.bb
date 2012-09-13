// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/string_piece.h"
#include "chrome/browser/extensions/api/web_request/form_data_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Attempts to run the parser corresponding to the |content_type_header|
// on the source represented by the concatenation of blocks from |bytes|.
// On success, returns true and the parsed |output|, else false.
// Parsed |output| has names on even positions (0, 2, ...), values on odd ones.
bool RunParser(const std::string& content_type_header,
               const std::vector<const base::StringPiece*>& bytes,
               std::vector<std::string>* output) {
  if (output == NULL)
    return false;
  output->clear();
  scoped_ptr<FormDataParser> parser(
      FormDataParser::Create(&content_type_header));
  if (parser.get() == NULL)
    return false;
  FormDataParser::Result result;
  for (size_t block = 0; block < bytes.size(); ++block) {
    if (!parser->SetSource(*(bytes[block])))
      return false;
    while (parser->GetNextNameValue(&result)) {
      output->push_back(result.name());
      output->push_back(result.value());
    }
  }
  return parser->AllDataReadOK();
}

}  // namespace

TEST(WebRequestFormDataParserTest, Parsing) {
  // We verify that POST data parsers cope with various formats of POST data.
  // Construct the test data.
#define kBoundary "THIS_IS_A_BOUNDARY"
#define kBlockStr1 "--" kBoundary "\r\n" \
    "Content-Disposition: form-data; name=\"text\"\r\n" \
    "\r\n" \
    "test\rtext\nwith non-CRLF line breaks\r\n" \
    "--" kBoundary "\r\n" \
    "Content-Disposition: form-data; name=\"file\"; filename=\"test\"\r\n" \
    "Content-Type: application/octet-stream\r\n" \
    "\r\n"
#define kBlockStr2 "\r\n" \
    "--" kBoundary "\r\n" \
    "Content-Disposition: form-data; name=\"password\"\r\n" \
    "\r\n" \
    "test password\r\n" \
    "--" kBoundary "\r\n" \
    "Content-Disposition: form-data; name=\"radio\"\r\n" \
    "\r\n" \
    "Yes\r\n" \
    "--" kBoundary "\r\n" \
    "Content-Disposition: form-data; name=\"check\"\r\n" \
    "\r\n" \
    "option A\r\n" \
    "--" kBoundary "\r\n" \
    "Content-Disposition: form-data; name=\"check\"\r\n" \
    "\r\n" \
    "option B\r\n" \
    "--" kBoundary "\r\n" \
    "Content-Disposition: form-data; name=\"txtarea\"\r\n" \
    "\r\n" \
    "Some text.\r\n" \
    "Other.\r\n" \
    "\r\n" \
    "--" kBoundary "\r\n" \
    "Content-Disposition: form-data; name=\"select\"\r\n" \
    "\r\n" \
    "one\r\n" \
    "--" kBoundary "--"
  // POST data input.
  const char kBigBlock[] = kBlockStr1 kBlockStr2;
  const char kBlock1[] = kBlockStr1;
  const char kBlock2[] = kBlockStr2;
  const char kUrlEncodedBlock[] = "text=test%0Dtext%0Awith+non-CRLF+line+breaks"
      "&file=test&password=test+password&radio=Yes&check=option+A"
      "&check=option+B&txtarea=Some+text.%0D%0AOther.%0D%0A&select=one";
#define BYTES_FROM_BLOCK(bytes, block) \
  const base::StringPiece bytes(block, sizeof(block) - 1)
  BYTES_FROM_BLOCK(kMultipartBytes, kBigBlock);
  BYTES_FROM_BLOCK(kMultipartBytesSplit1, kBlock1);
  BYTES_FROM_BLOCK(kMultipartBytesSplit2, kBlock2);
  BYTES_FROM_BLOCK(kUrlEncodedBytes, kUrlEncodedBlock);
  const char kPlainBlock[] = "abc";
  const base::StringPiece kTextPlainBytes(kPlainBlock, sizeof(kPlainBlock) - 1);
#undef BYTES_FROM_BLOCK
  // Headers.
  const char kUrlEncoded[] = "application/x-www-form-urlencoded";
  const char kTextPlain[] = "text/plain";
  const char kMultipart[] = "multipart/form-data; boundary=" kBoundary;
#undef kBlockStr2
#undef kBlockStr1
#undef kBoundary
  // Expected output.
  const char* kPairs[] = {
    "text", "test\rtext\nwith non-CRLF line breaks",
    "file", "test",
    "password", "test password",
    "radio", "Yes",
    "check", "option A",
    "check", "option B",
    "txtarea", "Some text.\r\nOther.\r\n",
    "select", "one"
  };
  const std::vector<std::string> kExpected(kPairs, kPairs + arraysize(kPairs));

  std::vector<const base::StringPiece*> input;
  std::vector<std::string> output;

  // First test: multipart POST data in one lump.
  input.push_back(&kMultipartBytes);
  EXPECT_TRUE(RunParser(kMultipart, input, &output));
  EXPECT_EQ(kExpected, output);

  // Second test: multipart POST data in several lumps.
  input.clear();
  input.push_back(&kMultipartBytesSplit1);
  input.push_back(&kMultipartBytesSplit2);
  EXPECT_TRUE(RunParser(kMultipart, input, &output));
  EXPECT_EQ(kExpected, output);

  // Third test: URL-encoded POST data.
  input.clear();
  input.push_back(&kUrlEncodedBytes);
  EXPECT_TRUE(RunParser(kUrlEncoded, input, &output));
  EXPECT_EQ(kExpected, output);

  // Fourth test: text/plain POST data in one lump.
  input.clear();
  input.push_back(&kTextPlainBytes);
  // This should fail, text/plain is ambiguous and thus unparseable.
  EXPECT_FALSE(RunParser(kTextPlain, input, &output));
}

}  // namespace extensions
