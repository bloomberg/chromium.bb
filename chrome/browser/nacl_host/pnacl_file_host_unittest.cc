// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/pnacl_file_host.h"

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"

#include "testing/gtest/include/gtest/gtest.h"

using pnacl_file_host::PnaclCanOpenFile;

// Try to pass a few funny filenames with a dummy pnacl directory set.
TEST(PnaclFileHostTest, TestFilenamesWithPnaclPath) {
  base::FilePath kDummyPnaclPath(FILE_PATH_LITERAL("dummy_pnacl_path"));
  base::ScopedPathOverride pnach_dir_override(chrome::DIR_PNACL_COMPONENT,
                                              kDummyPnaclPath);
  ASSERT_TRUE(PathService::Get(chrome::DIR_PNACL_COMPONENT,
                               &kDummyPnaclPath));

  // Check allowed strings, and check that the expected prefix is added.
  base::FilePath out_path;
  EXPECT_TRUE(PnaclCanOpenFile("pnacl_json", &out_path));
  base::FilePath expected_path = kDummyPnaclPath.Append(
      FILE_PATH_LITERAL("pnacl_public_pnacl_json"));
  EXPECT_EQ(out_path, expected_path);

  EXPECT_TRUE(PnaclCanOpenFile("x86_32_llc", &out_path));
  expected_path = kDummyPnaclPath.Append(
      FILE_PATH_LITERAL("pnacl_public_x86_32_llc"));
  EXPECT_EQ(out_path, expected_path);

  // Check character ranges.
  EXPECT_FALSE(PnaclCanOpenFile(".xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("/xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("x/chars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("\\xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("x\\chars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("$xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("%xchars", &out_path));
  EXPECT_FALSE(PnaclCanOpenFile("CAPS", &out_path));
  const char non_ascii[] = "\xff\xfe";
  EXPECT_FALSE(PnaclCanOpenFile(non_ascii, &out_path));

  // Check file length restriction.
  EXPECT_FALSE(PnaclCanOpenFile("thisstringisactuallywaaaaaaaaaaaaaaaaaaaaaaaa"
                                "toolongwaytoolongwaaaaayyyyytoooooooooooooooo"
                                "looooooooong",
                                &out_path));

  // Other bad files.
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

}
