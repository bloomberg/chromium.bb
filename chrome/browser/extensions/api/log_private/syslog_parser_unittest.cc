// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/browser/extensions/api/log_private/log_parser.h"
#include "chrome/browser/extensions/api/log_private/syslog_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

const char kShillLogEntry[] =
    "2013-07-08T11:28:12.440308+02:00 localhost shill:"
    "[0708/112812:ERROR:manager.cc(480)] Skipping unload of service";

const char kWpaSupplicantLogEntry[] =
    "2013-07-18T12:39:07.443100-07:00 localhost wpa_supplicant[894]:"
    "dbus: Failed to construct signal";

}  // namespace

class ExtensionSyslogParserTest : public testing::Test {
};

TEST_F(ExtensionSyslogParserTest, ParseLog) {
  std::vector<linked_ptr<api::log_private::LogEntry> > output;
  api::log_private::Filter filter;
  FilterHandler filter_handler(filter);
  SyslogParser p;
  // Test shill log
  p.Parse(kShillLogEntry, &output, &filter_handler);
  ASSERT_EQ(1u, output.size());
  EXPECT_STREQ("error", output[0]->level.c_str());
  EXPECT_STREQ("shill:", output[0]->process.c_str());
  EXPECT_STREQ("unknown", output[0]->process_id.c_str());
  EXPECT_STREQ(kShillLogEntry, output[0]->full_entry.c_str());
  EXPECT_DOUBLE_EQ(1373275692440.308, output[0]->timestamp);

  // Test WpaSupplicant log
  p.Parse(kWpaSupplicantLogEntry, &output, &filter_handler);
  ASSERT_EQ(2u, output.size());
  EXPECT_STREQ("unknown", output[1]->level.c_str());
  EXPECT_STREQ("wpa_supplicant", output[1]->process.c_str());
  EXPECT_STREQ("894", output[1]->process_id.c_str());
  EXPECT_STREQ(kWpaSupplicantLogEntry, output[1]->full_entry.c_str());
  EXPECT_DOUBLE_EQ(1374176347443.1, output[1]->timestamp);
}

}  // namespace extensions
