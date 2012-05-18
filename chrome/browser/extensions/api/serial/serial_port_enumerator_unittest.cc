// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/extensions/api/serial/serial_port_enumerator.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SerialPortEnumeratorTest, ValidPortNamePatterns) {
#if defined(OS_WIN)
    // TODO(miket): implement
#else
  const extensions::SerialPortEnumerator::StringSet port_patterns(
      extensions::SerialPortEnumerator::GenerateValidPatterns());

  const char *kValidNames[] = {
    "/dev/867serial5309",
    "/dev/tty.usbmodem999999",
    "/dev/ttyACM0",
    "/dev/xxxxxmodemxxxxxxxxxx",
  };

  for (size_t i = 0; i < arraysize(kValidNames); ++i) {
    std::set<std::string>::const_iterator j = port_patterns.begin();
    for (; j != port_patterns.end(); ++j) {
      if (MatchPattern(kValidNames[i], *j)) {
        break;
      }
    }
    EXPECT_TRUE(j != port_patterns.end()) << kValidNames[i] <<
        " should have matched at least one pattern";
  }
#endif
}

TEST(SerialPortEnumeratorTest, InvalidPortNamePatterns) {
  const extensions::SerialPortEnumerator::StringSet port_patterns(
      extensions::SerialPortEnumerator::GenerateValidPatterns());

  const char *kInvalidNames[] = {
#if defined(OS_WIN)
    "/dev/ttyACM0",
#else
    "",
    ".",
    "..",
    "//COM0",
    "/dev/../home/you/your_secrets",
    "/dev/cdrom",
    "/dev/laserbeam",
    "/home/you/your_secrets",
    "/proc/cpuinfo",
    "COM0",
    "\\\\COM0",
    "modem",
    "serial",
    "tty",
#endif
  };

  for (size_t i = 0; i < arraysize(kInvalidNames); ++i) {
    std::set<std::string>::const_iterator j = port_patterns.begin();
    for (; j != port_patterns.end(); ++j)
      EXPECT_FALSE(MatchPattern(kInvalidNames[i], *j)) <<
          kInvalidNames[i] << " should not have matched " << *j;
  }
}
