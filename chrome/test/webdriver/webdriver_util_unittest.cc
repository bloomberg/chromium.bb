// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/base64.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/test/webdriver/webdriver_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webdriver {

TEST(RandomIDTest, CanGenerateSufficientlyRandomIDs) {
  std::set<std::string> generated_ids;
  for (int i = 0; i < 10000; ++i) {
    std::string id = GenerateRandomID();
    ASSERT_EQ(32u, id.length());
    ASSERT_TRUE(generated_ids.end() == generated_ids.find(id))
        << "Generated duplicate ID: " << id
        << " on iteration " << i;
    generated_ids.insert(id);
  }
}

TEST(ZipFileTest, ZipEntryToZipArchive) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string data;
  // A zip entry sent from a Java WebDriver client (v2.20) that contains a
  // file with the contents "COW\n".
  const char* kBase64ZipEntry =
      "UEsDBBQACAAIAJpyXEAAAAAAAAAAAAAAAAAEAAAAdGVzdHP2D+"
      "cCAFBLBwi/wAzGBgAAAAQAAAA=";
  ASSERT_TRUE(base::Base64Decode(kBase64ZipEntry, &data));
  base::FilePath file;
  std::string error_msg;
  ASSERT_TRUE(UnzipSoleFile(temp_dir.path(), data, &file, &error_msg))
      << error_msg;
  std::string contents;
  ASSERT_TRUE(file_util::ReadFileToString(file, &contents));
  ASSERT_STREQ("COW\n", contents.c_str());
}

}  // namespace webdriver
