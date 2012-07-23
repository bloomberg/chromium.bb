// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/pnacl_file_host.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

#include "testing/gtest/include/gtest/gtest.h"

using pnacl_file_host::PnaclCanOpenFile;

// Try to pass a few funny filenames with a dummy pnacl directory set.
TEST(PnaclFileHostTest, TestFilenamesWithPnaclPath) {
  FilePath kDummyPnaclPath(FILE_PATH_LITERAL("dummy_pnacl_path"));
  ASSERT_TRUE(PathService::Override(chrome::DIR_PNACL_COMPONENT,
                                    kDummyPnaclPath));
  ASSERT_TRUE(PathService::Get(chrome::DIR_PNACL_COMPONENT,
                               &kDummyPnaclPath));
  FilePath out_path;
  EXPECT_TRUE(PnaclCanOpenFile("manifest.json", &out_path));
  FilePath expected_path = kDummyPnaclPath.Append(
      FILE_PATH_LITERAL("manifest.json"));
  EXPECT_EQ(out_path, expected_path);

  EXPECT_TRUE(PnaclCanOpenFile("x86-32/llc", &out_path));
  expected_path = kDummyPnaclPath.Append(FILE_PATH_LITERAL("x86-32/llc"));
  EXPECT_EQ(out_path, expected_path);

  EXPECT_FALSE(PnaclCanOpenFile("", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile(".", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("..", &out_path));
#if defined(OS_WIN)
  EXPECT_FALSE(PnaclCanOpenFile("..\\llc", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%SystemRoot%", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%SystemRoot%\\explorer.exe", &out_path));
#else
  EXPECT_FALSE(PnaclCanOpenFile("../llc", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("/bin/sh", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$HOME", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$HOME/.bashrc", &out_path));
#endif

  const char non_ascii[] = "\xff\xfe";
  EXPECT_FALSE(PnaclCanOpenFile(non_ascii, &out_path));
}
