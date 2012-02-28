// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64.h"
#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/common/metrics/metrics_log_base.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace {

// Since buildtime is highly variable, this function will scan an output log and
// replace it with a consistent number.
void NormalizeBuildtime(std::string* xml_encoded) {
  std::string prefix = "buildtime=\"";
  const char postfix = '\"';
  size_t offset = xml_encoded->find(prefix);
  ASSERT_NE(std::string::npos, offset);
  offset += prefix.size();
  size_t postfix_position = xml_encoded->find(postfix, offset);
  ASSERT_NE(std::string::npos, postfix_position);
  for (size_t i = offset; i < postfix_position; ++i) {
    char digit = xml_encoded->at(i);
    ASSERT_GE(digit, '0');
    ASSERT_LE(digit, '9');
  }

  // Insert a single fake buildtime.
  xml_encoded->replace(offset, postfix_position - offset, "123246");
}

class NoTimeMetricsLogBase : public MetricsLogBase {
 public:
  NoTimeMetricsLogBase(std::string client_id,
                       int session_id,
                       std::string version_string):
      MetricsLogBase(client_id, session_id, version_string) {}
  virtual ~NoTimeMetricsLogBase() {}

  // Override this so that output is testable.
  virtual std::string GetCurrentTimeString() OVERRIDE {
    return std::string();
  }
};

}  // namespace

TEST(MetricsLogBaseTest, EmptyRecord) {
  std::string expected_output =
#if defined(OS_CHROMEOS)
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
      "appversion=\"bogus version\" hardwareclass=\"sample-class\"/>";
#else
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
      "appversion=\"bogus version\"/>";
#endif  // OS_CHROMEOS

  MetricsLogBase log("bogus client ID", 0, "bogus version");
  log.set_hardware_class("sample-class");
  log.CloseLog();

  int size = log.GetEncodedLogSizeXml();
  ASSERT_GT(size, 0);

  std::string encoded;
  // Leave room for the NUL terminator.
  ASSERT_TRUE(log.GetEncodedLogXml(WriteInto(&encoded, size + 1), size));
  TrimWhitespaceASCII(encoded, TRIM_ALL, &encoded);
  NormalizeBuildtime(&encoded);
  NormalizeBuildtime(&expected_output);

  ASSERT_EQ(expected_output, encoded);
}

TEST(MetricsLogBaseTest, WindowEvent) {
  std::string expected_output =
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
          "appversion=\"bogus version\">\n"
      " <window action=\"create\" windowid=\"0\" session=\"0\" time=\"\"/>\n"
      " <window action=\"open\" windowid=\"1\" parent=\"0\" "
          "session=\"0\" time=\"\"/>\n"
      " <window action=\"close\" windowid=\"1\" parent=\"0\" "
          "session=\"0\" time=\"\"/>\n"
      " <window action=\"destroy\" windowid=\"0\" session=\"0\" time=\"\"/>\n"
      "</log>";

  NoTimeMetricsLogBase log("bogus client ID", 0, "bogus version");
  log.RecordWindowEvent(MetricsLogBase::WINDOW_CREATE, 0, -1);
  log.RecordWindowEvent(MetricsLogBase::WINDOW_OPEN, 1, 0);
  log.RecordWindowEvent(MetricsLogBase::WINDOW_CLOSE, 1, 0);
  log.RecordWindowEvent(MetricsLogBase::WINDOW_DESTROY, 0, -1);
  log.CloseLog();

  ASSERT_EQ(4, log.num_events());

  int size = log.GetEncodedLogSizeXml();
  ASSERT_GT(size, 0);

  std::string encoded;
  // Leave room for the NUL terminator.
  ASSERT_TRUE(log.GetEncodedLogXml(WriteInto(&encoded, size + 1), size));
  TrimWhitespaceASCII(encoded, TRIM_ALL, &encoded);
  NormalizeBuildtime(&encoded);
  NormalizeBuildtime(&expected_output);

  ASSERT_EQ(expected_output, encoded);
}

TEST(MetricsLogBaseTest, LoadEvent) {
  std::string expected_output =
#if defined(OS_CHROMEOS)
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
          "appversion=\"bogus version\" hardwareclass=\"sample-class\">\n"
      " <document action=\"load\" docid=\"1\" window=\"3\" loadtime=\"7219\" "
          "origin=\"link\" session=\"0\" time=\"\"/>\n"
      "</log>";
#else
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
          "appversion=\"bogus version\">\n"
      " <document action=\"load\" docid=\"1\" window=\"3\" loadtime=\"7219\" "
          "origin=\"link\" session=\"0\" time=\"\"/>\n"
      "</log>";
#endif  // OS_CHROMEOS

  NoTimeMetricsLogBase log("bogus client ID", 0, "bogus version");
  log.RecordLoadEvent(3, GURL("http://google.com"),
                      content::PAGE_TRANSITION_LINK, 1,
                      TimeDelta::FromMilliseconds(7219));
  log.set_hardware_class("sample-class");
  log.CloseLog();

  ASSERT_EQ(1, log.num_events());

  int size = log.GetEncodedLogSizeXml();
  ASSERT_GT(size, 0);

  std::string encoded;
  // Leave room for the NUL terminator.
  ASSERT_TRUE(log.GetEncodedLogXml(WriteInto(&encoded, size + 1), size));
  TrimWhitespaceASCII(encoded, TRIM_ALL, &encoded);
  NormalizeBuildtime(&encoded);
  NormalizeBuildtime(&expected_output);

  ASSERT_EQ(expected_output, encoded);
}

// Make sure our ID hashes are the same as what we see on the server side.
TEST(MetricsLogBaseTest, CreateHashes) {
  static const struct {
    std::string input;
    std::string output;
  } cases[] = {
    {"Back", "0x0557fa923dcee4d0"},
    {"Forward", "0x67d2f6740a8eaebf"},
    {"NewTab", "0x290eb683f96572f1"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    std::string hash_string;
    uint64 hash_numeric;
    MetricsLogBase::CreateHashes(cases[i].input, &hash_string, &hash_numeric);

    // Verify the numeric representation.
    {
      std::string hash_numeric_hex =
          base::StringPrintf("0x%016" PRIx64, hash_numeric);
      EXPECT_EQ(cases[i].output, hash_numeric_hex);
    }

    // Verify the string representation.
    {
      std::string hash_string_decoded;
      ASSERT_TRUE(base::Base64Decode(hash_string, &hash_string_decoded));

      // Convert to hex string
      // We're only checking the first 8 bytes, because that's what
      // the metrics server uses.
      std::string hash_string_hex = "0x";
      ASSERT_GE(hash_string_decoded.size(), 8U);
      for (size_t j = 0; j < 8; j++) {
        base::StringAppendF(&hash_string_hex, "%02x",
                            static_cast<uint8>(hash_string_decoded.data()[j]));
      }
      EXPECT_EQ(cases[i].output, hash_string_hex);
    }
  }
};
