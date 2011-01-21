// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace {
  class MetricsLogTest : public testing::Test {
  };
};


// Since buildtime is highly variable, this function will scan an output log and
// replace it with a consistent number.
static void NormalizeBuildtime(std::string* xml_encoded) {
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

TEST(MetricsLogTest, EmptyRecord) {
  std::string expected_output = StringPrintf(
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
      "appversion=\"%s\"/>", MetricsLog::GetVersionString().c_str());

  MetricsLog log("bogus client ID", 0);
  log.CloseLog();

  int size = log.GetEncodedLogSize();
  ASSERT_GT(size, 0);

  std::string encoded;
  // Leave room for the NUL terminator.
  ASSERT_TRUE(log.GetEncodedLog(WriteInto(&encoded, size + 1), size));
  TrimWhitespaceASCII(encoded, TRIM_ALL, &encoded);
  NormalizeBuildtime(&encoded);
  NormalizeBuildtime(&expected_output);

  ASSERT_EQ(expected_output, encoded);
}

#if defined(OS_CHROMEOS)
TEST(MetricsLogTest, ChromeOSEmptyRecord) {
  std::string expected_output = StringPrintf(
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
      "appversion=\"%s\" hardwareclass=\"sample-class\"/>",
      MetricsLog::GetVersionString().c_str());

  MetricsLog log("bogus client ID", 0);
  log.set_hardware_class("sample-class");
  log.CloseLog();

  int size = log.GetEncodedLogSize();
  ASSERT_GT(size, 0);

  std::string encoded;
  // Leave room for the NUL terminator.
  ASSERT_TRUE(log.GetEncodedLog(WriteInto(&encoded, size + 1), size));
  TrimWhitespaceASCII(encoded, TRIM_ALL, &encoded);
  NormalizeBuildtime(&encoded);
  NormalizeBuildtime(&expected_output);

  ASSERT_EQ(expected_output, encoded);
}
#endif  // OS_CHROMEOS

namespace {

class NoTimeMetricsLog : public MetricsLog {
 public:
  NoTimeMetricsLog(std::string client_id, int session_id):
      MetricsLog(client_id, session_id) {}
  virtual ~NoTimeMetricsLog() {}

  // Override this so that output is testable.
  virtual std::string GetCurrentTimeString() {
    return std::string();
  }
};

};  // namespace

TEST(MetricsLogTest, WindowEvent) {
  std::string expected_output = StringPrintf(
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
          "appversion=\"%s\">\n"
      " <window action=\"create\" windowid=\"0\" session=\"0\" time=\"\"/>\n"
      " <window action=\"open\" windowid=\"1\" parent=\"0\" "
          "session=\"0\" time=\"\"/>\n"
      " <window action=\"close\" windowid=\"1\" parent=\"0\" "
          "session=\"0\" time=\"\"/>\n"
      " <window action=\"destroy\" windowid=\"0\" session=\"0\" time=\"\"/>\n"
      "</log>", MetricsLog::GetVersionString().c_str());

  NoTimeMetricsLog log("bogus client ID", 0);
  log.RecordWindowEvent(MetricsLog::WINDOW_CREATE, 0, -1);
  log.RecordWindowEvent(MetricsLog::WINDOW_OPEN, 1, 0);
  log.RecordWindowEvent(MetricsLog::WINDOW_CLOSE, 1, 0);
  log.RecordWindowEvent(MetricsLog::WINDOW_DESTROY, 0, -1);
  log.CloseLog();

  ASSERT_EQ(4, log.num_events());

  int size = log.GetEncodedLogSize();
  ASSERT_GT(size, 0);

  std::string encoded;
  // Leave room for the NUL terminator.
  ASSERT_TRUE(log.GetEncodedLog(WriteInto(&encoded, size + 1), size));
  TrimWhitespaceASCII(encoded, TRIM_ALL, &encoded);
  NormalizeBuildtime(&encoded);
  NormalizeBuildtime(&expected_output);

  ASSERT_EQ(expected_output, encoded);
}

TEST(MetricsLogTest, LoadEvent) {
  std::string expected_output = StringPrintf(
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
          "appversion=\"%s\">\n"
      " <document action=\"load\" docid=\"1\" window=\"3\" loadtime=\"7219\" "
          "origin=\"link\" session=\"0\" time=\"\"/>\n"
      "</log>", MetricsLog::GetVersionString().c_str());

  NoTimeMetricsLog log("bogus client ID", 0);
  log.RecordLoadEvent(3, GURL("http://google.com"), PageTransition::LINK,
                      1, TimeDelta::FromMilliseconds(7219));

  log.CloseLog();

  ASSERT_EQ(1, log.num_events());

  int size = log.GetEncodedLogSize();
  ASSERT_GT(size, 0);

  std::string encoded;
  // Leave room for the NUL terminator.
  ASSERT_TRUE(log.GetEncodedLog(WriteInto(&encoded, size + 1), size));
  TrimWhitespaceASCII(encoded, TRIM_ALL, &encoded);
  NormalizeBuildtime(&encoded);
  NormalizeBuildtime(&expected_output);

  ASSERT_EQ(expected_output, encoded);
}

#if defined(OS_CHROMEOS)
TEST(MetricsLogTest, ChromeOSLoadEvent) {
  std::string expected_output = StringPrintf(
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
          "appversion=\"%s\" hardwareclass=\"sample-class\">\n"
      " <document action=\"load\" docid=\"1\" window=\"3\" loadtime=\"7219\" "
          "origin=\"link\" session=\"0\" time=\"\"/>\n"
      "</log>", MetricsLog::GetVersionString().c_str());

  NoTimeMetricsLog log("bogus client ID", 0);
  log.RecordLoadEvent(3, GURL("http://google.com"), PageTransition::LINK,
                      1, TimeDelta::FromMilliseconds(7219));
  log.set_hardware_class("sample-class");
  log.CloseLog();

  ASSERT_EQ(1, log.num_events());

  int size = log.GetEncodedLogSize();
  ASSERT_GT(size, 0);

  std::string encoded;
  // Leave room for the NUL terminator.
  ASSERT_TRUE(log.GetEncodedLog(WriteInto(&encoded, size + 1), size));
  TrimWhitespaceASCII(encoded, TRIM_ALL, &encoded);
  NormalizeBuildtime(&encoded);
  NormalizeBuildtime(&expected_output);

  ASSERT_EQ(expected_output, encoded);
}

TEST(MetricsLogTest, ChromeOSStabilityData) {
  NoTimeMetricsLog log("bogus client ID", 0);
  TestingProfile profile;
  PrefService* pref = profile.GetPrefs();

  pref->SetInteger(prefs::kStabilityChildProcessCrashCount, 10);
  pref->SetInteger(prefs::kStabilityOtherUserCrashCount, 11);
  pref->SetInteger(prefs::kStabilityKernelCrashCount, 12);
  pref->SetInteger(prefs::kStabilitySystemUncleanShutdownCount, 13);
  std::string expected_output = StringPrintf(
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
          "appversion=\"%s\">\n"
      "<stability stuff>\n", MetricsLog::GetVersionString().c_str());
  // Expect 3 warnings about not yet being able to send the
  // Chrome OS stability stats.
  log.WriteStabilityElement(profile.GetPrefs());
  log.CloseLog();

  int size = log.GetEncodedLogSize();
  ASSERT_GT(size, 0);

  EXPECT_EQ(0, pref->GetInteger(prefs::kStabilityChildProcessCrashCount));
  EXPECT_EQ(0, pref->GetInteger(prefs::kStabilityOtherUserCrashCount));
  EXPECT_EQ(0, pref->GetInteger(prefs::kStabilityKernelCrashCount));
  EXPECT_EQ(0, pref->GetInteger(prefs::kStabilitySystemUncleanShutdownCount));

  std::string encoded;
  // Leave room for the NUL terminator.
  bool encoding_result = log.GetEncodedLog(
      WriteInto(&encoded, size + 1), size);
  ASSERT_TRUE(encoding_result);

  // Check that we can find childprocesscrashcount, but not
  // any of the ChromeOS ones that we are not emitting until log
  // servers can handle them.
  EXPECT_NE(std::string::npos,
            encoded.find(" childprocesscrashcount=\"10\""));
  EXPECT_EQ(std::string::npos,
            encoded.find(" otherusercrashcount="));
  EXPECT_EQ(std::string::npos,
            encoded.find(" kernelcrashcount="));
  EXPECT_EQ(std::string::npos,
            encoded.find(" systemuncleanshutdowns="));
}

#endif  // OS_CHROMEOS

// Make sure our ID hashes are the same as what we see on the server side.
TEST(MetricsLogTest, CreateHash) {
  static const struct {
    std::string input;
    std::string output;
  } cases[] = {
    {"Back", "0x0557fa923dcee4d0"},
    {"Forward", "0x67d2f6740a8eaebf"},
    {"NewTab", "0x290eb683f96572f1"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    std::string hash_string = MetricsLog::CreateHash(cases[i].input);

    // Convert to hex string
    // We're only checking the first 8 bytes, because that's what
    // the metrics server uses.
    std::string hash_hex = "0x";
    for (size_t j = 0; j < 8; j++) {
      base::StringAppendF(&hash_hex, "%02x",
                          static_cast<uint8>(hash_string.data()[j]));
    }
    EXPECT_EQ(cases[i].output, hash_hex);
  }
};
