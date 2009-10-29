// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_io.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BlacklistIOTest, Generic) {
  // Testing data path.
  FilePath data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &data_dir);

  FilePath input = data_dir.AppendASCII("blacklist_small.pbl");

  FilePath expected = data_dir.AppendASCII("blacklist_small.pbr");

  Blacklist blacklist;
  std::string error_string;
  ASSERT_TRUE(BlacklistIO::ReadText(&blacklist, input, &error_string));
  EXPECT_TRUE(error_string.empty());
  
  const Blacklist::EntryList entries(blacklist.entries_begin(),
                                     blacklist.entries_end());
  ASSERT_EQ(5U, entries.size());

  EXPECT_EQ("@", entries[0]->pattern());
  EXPECT_EQ("@poor-security-site.com", entries[1]->pattern());
  EXPECT_EQ("@.ad-serving-place.com", entries[2]->pattern());
  EXPECT_EQ("www.site.com/anonymous/folder/@", entries[3]->pattern());
  EXPECT_EQ("www.site.com/bad/url", entries[4]->pattern());
  
  const Blacklist::ProviderList providers(blacklist.providers_begin(),
                                          blacklist.providers_end());

  ASSERT_EQ(1U, providers.size());
  EXPECT_EQ("Sample", providers[0]->name());
  EXPECT_EQ("http://www.google.com", providers[0]->url());

  FilePath output;
  PathService::Get(base::DIR_TEMP, &output);
  output = output.AppendASCII("blacklist_small.pbr");
  ASSERT_TRUE(BlacklistIO::WriteBinary(&blacklist, output));
  EXPECT_TRUE(file_util::ContentsEqual(output, expected));
  EXPECT_TRUE(file_util::Delete(output, false));
}


