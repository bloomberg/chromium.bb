// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_io.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BlacklistIOTest, Generic) {
  // Testing data path.
  std::wstring data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &data_dir);

  std::wstring input(data_dir);
  file_util::AppendToPath(&input, L"blacklist_small.pbl");

  std::wstring expected(data_dir);
  file_util::AppendToPath(&expected, L"blacklist_small.pbr");

  BlacklistIO io;
  EXPECT_TRUE(io.Read(FilePath::FromWStringHack(input)));
  const std::list<Blacklist::Entry*>& blacklist = io.blacklist();
  EXPECT_EQ(5U, blacklist.size());

  std::list<Blacklist::Entry*>::const_iterator i = blacklist.begin();
  EXPECT_EQ("@", (*i++)->pattern());
  EXPECT_EQ("@poor-security-site.com", (*i++)->pattern());
  EXPECT_EQ("@.ad-serving-place.com", (*i++)->pattern());
  EXPECT_EQ("www.site.com/anonymous/folder/@", (*i++)->pattern());
  EXPECT_EQ("www.site.com/bad/url", (*i++)->pattern());

  EXPECT_EQ(1U, io.providers().size());
  EXPECT_EQ("Sample", io.providers().front()->name());
  EXPECT_EQ("http://www.google.com", io.providers().front()->url());

  std::wstring output;
  PathService::Get(base::DIR_TEMP, &output);
  file_util::AppendToPath(&output, L"blacklist_small.pbr");
  CHECK(io.Write(FilePath::FromWStringHack(output)));
  EXPECT_TRUE(file_util::ContentsEqual(output, expected));
  EXPECT_TRUE(file_util::Delete(output, false));
}
