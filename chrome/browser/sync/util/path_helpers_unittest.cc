// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/port.h"
#include "build/build_config.h"
#include "chrome/browser/sync/syncable/path_name_cmp.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/sync_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncable {

class PathHelpersTest : public testing::Test {
};

TEST(PathHelpersTest, SanitizePathComponent) {
#if defined(OS_WIN)
  EXPECT_EQ(MakePathComponentOSLegal("bar"), "");
  EXPECT_EQ(MakePathComponentOSLegal("bar <"), "bar");
  EXPECT_EQ(MakePathComponentOSLegal("bar.<"), "bar");
  EXPECT_EQ(MakePathComponentOSLegal("prn"), "prn~1");
  EXPECT_EQ(MakePathComponentOSLegal("pr>n"), "prn~1");
  EXPECT_EQ(MakePathComponentOSLegal("ab:c"), "abc");
  EXPECT_EQ(MakePathComponentOSLegal("a|bc"), "abc");
  EXPECT_EQ(MakePathComponentOSLegal("baz~9"), "");
  EXPECT_EQ(MakePathComponentOSLegal("\007"), "~1");
  EXPECT_EQ(MakePathComponentOSLegal("com1.txt.bat"), "com1~1.txt.bat");
  EXPECT_EQ(MakePathComponentOSLegal("foo.com1.bat"), "");
  EXPECT_EQ(MakePathComponentOSLegal("\010gg"), "gg");
  EXPECT_EQ(MakePathComponentOSLegal("<"), "~1");
  EXPECT_EQ(MakePathComponentOSLegal("col:on"), "colon");
  EXPECT_EQ(MakePathComponentOSLegal("q\""), "q");
  EXPECT_EQ(MakePathComponentOSLegal("back\\slAsh"), "backslAsh");
  EXPECT_EQ(MakePathComponentOSLegal("sla/sh "), "slash");
  EXPECT_EQ(MakePathComponentOSLegal("s|laSh"), "slaSh");
  EXPECT_EQ(MakePathComponentOSLegal("CON"), "CON~1");
  EXPECT_EQ(MakePathComponentOSLegal("PRN"), "PRN~1");
  EXPECT_EQ(MakePathComponentOSLegal("AUX"), "AUX~1");
  EXPECT_EQ(MakePathComponentOSLegal("NUL"), "NUL~1");
  EXPECT_EQ(MakePathComponentOSLegal("COM1"), "COM1~1");
  EXPECT_EQ(MakePathComponentOSLegal("COM2"), "COM2~1");
  EXPECT_EQ(MakePathComponentOSLegal("COM3"), "COM3~1");
  EXPECT_EQ(MakePathComponentOSLegal("COM4"), "COM4~1");
  EXPECT_EQ(MakePathComponentOSLegal("COM5"), "COM5~1");
  EXPECT_EQ(MakePathComponentOSLegal("COM6"), "COM6~1");
  EXPECT_EQ(MakePathComponentOSLegal("COM7"), "COM7~1");
  EXPECT_EQ(MakePathComponentOSLegal("COM8"), "COM8~1");
  EXPECT_EQ(MakePathComponentOSLegal("COM9"), "COM9~1");
  EXPECT_EQ(MakePathComponentOSLegal("LPT1"), "LPT1~1");
  EXPECT_EQ(MakePathComponentOSLegal("LPT2"), "LPT2~1");
  EXPECT_EQ(MakePathComponentOSLegal("LPT3"), "LPT3~1");
  EXPECT_EQ(MakePathComponentOSLegal("LPT4"), "LPT4~1");
  EXPECT_EQ(MakePathComponentOSLegal("LPT5"), "LPT5~1");
  EXPECT_EQ(MakePathComponentOSLegal("LPT6"), "LPT6~1");
  EXPECT_EQ(MakePathComponentOSLegal("LPT7"), "LPT7~1");
  EXPECT_EQ(MakePathComponentOSLegal("LPT8"), "LPT8~1");
  EXPECT_EQ(MakePathComponentOSLegal("LPT9"), "LPT9~1");
  EXPECT_EQ(MakePathComponentOSLegal("bar~bar"), "");
  EXPECT_EQ(MakePathComponentOSLegal("adlr~-3"), "");
  EXPECT_EQ(MakePathComponentOSLegal("tilde~"), "");
  EXPECT_EQ(MakePathComponentOSLegal("mytext.txt"), "");
  EXPECT_EQ(MakePathComponentOSLegal("mytext|.txt"), "mytext.txt");
  EXPECT_EQ(MakePathComponentOSLegal("okay.com1.txt"), "");
  EXPECT_EQ(MakePathComponentOSLegal("software-3.tar.gz"), "");
  EXPECT_EQ(MakePathComponentOSLegal("<"), "~1");
  EXPECT_EQ(MakePathComponentOSLegal("<.<"), "~1");
  EXPECT_EQ(MakePathComponentOSLegal("<.<txt"), ".txt");
  EXPECT_EQ(MakePathComponentOSLegal("txt<.<"), "txt");
#else  // !defined(OS_WIN)
  EXPECT_EQ(MakePathComponentOSLegal("bar"), "");
  EXPECT_EQ(MakePathComponentOSLegal("b"), "");
  EXPECT_EQ(MakePathComponentOSLegal("A"), "");
  EXPECT_EQ(MakePathComponentOSLegal("<'|"), "");
  EXPECT_EQ(MakePathComponentOSLegal("/"), ":");
  EXPECT_EQ(MakePathComponentOSLegal(":"), "");
#endif  // defined(OS_WIN)
}

}  // namespace syncable
