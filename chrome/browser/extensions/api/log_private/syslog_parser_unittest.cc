// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/browser/extensions/api/log_private/log_private_api.h"
#include "chrome/browser/extensions/api/log_private/syslog_parser.h"
#include "chrome/common/extensions/api/log_private.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

const char kShillLogEntry[] =
    "2013-07-08T11:28:12.440308-07:00 localhost shill:"
    "[0708/112812:ERROR:manager.cc(480)] Skipping unload of service";

const char kWpaSupplicantLogEntry[] =
    "2013-07-08T12:39:07.443100-07:00 localhost wpa_supplicant[894]:"
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
  EXPECT_STREQ(output[0]->level.c_str(), "error");
  EXPECT_STREQ(output[0]->process.c_str(), "shill:");
  EXPECT_STREQ(output[0]->process_id.c_str(), "unknown");
  EXPECT_STREQ(output[0]->full_entry.c_str(), kShillLogEntry);
  EXPECT_EQ(output[0]->timestamp, 1373308092440.308);
  // Test WpaSupplicant log
  p.Parse(kWpaSupplicantLogEntry, &output, &filter_handler);
  EXPECT_STREQ(output[1]->level.c_str(), "unknown");
  EXPECT_STREQ(output[1]->process.c_str(), "wpa_supplicant");
  EXPECT_STREQ(output[1]->process_id.c_str(), "894");
  EXPECT_STREQ(output[1]->full_entry.c_str(), kWpaSupplicantLogEntry);
  EXPECT_EQ(output[1]->timestamp, 1373312347443.1);
}

}  // namespace extensions
