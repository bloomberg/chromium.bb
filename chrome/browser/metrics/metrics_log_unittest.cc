// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
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

}  // namespace

class MetricsLogTest : public testing::Test {
};

#if defined(OS_CHROMEOS)
TEST(MetricsLogTest, ChromeOSStabilityData) {
  NoTimeMetricsLog log("bogus client ID", 0);
  TestingPrefService prefs;
  browser::RegisterLocalState(&prefs);

  prefs.SetInteger(prefs::kStabilityChildProcessCrashCount, 10);
  prefs.SetInteger(prefs::kStabilityOtherUserCrashCount, 11);
  prefs.SetInteger(prefs::kStabilityKernelCrashCount, 12);
  prefs.SetInteger(prefs::kStabilitySystemUncleanShutdownCount, 13);
  std::string expected_output = base::StringPrintf(
      "<log clientid=\"bogus client ID\" buildtime=\"123456789\" "
          "appversion=\"%s\">\n"
      "<stability stuff>\n", MetricsLog::GetVersionString().c_str());
  // Expect 3 warnings about not yet being able to send the
  // Chrome OS stability stats.
  log.WriteStabilityElement(&prefs);
  log.CloseLog();

  int size = log.GetEncodedLogSizeXml();
  ASSERT_GT(size, 0);

  EXPECT_EQ(0, prefs.GetInteger(prefs::kStabilityChildProcessCrashCount));
  EXPECT_EQ(0, prefs.GetInteger(prefs::kStabilityOtherUserCrashCount));
  EXPECT_EQ(0, prefs.GetInteger(prefs::kStabilityKernelCrashCount));
  EXPECT_EQ(0, prefs.GetInteger(prefs::kStabilitySystemUncleanShutdownCount));

  std::string encoded;
  // Leave room for the NUL terminator.
  bool encoding_result = log.GetEncodedLogXml(
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
