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
  FilePath data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &data_dir);

  FilePath input =
      data_dir.Append(FilePath::FromWStringHack(L"blacklist_small.pbl"));

  FilePath expected =
      data_dir.Append(FilePath::FromWStringHack(L"blacklist_small.pbr"));

  BlacklistIO io;
  EXPECT_TRUE(io.Read(input));
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

  FilePath output;
  PathService::Get(base::DIR_TEMP, &output);
  output = output.Append(FilePath::FromWStringHack(L"blacklist_small.pbr"));
  CHECK(io.Write(output));
  EXPECT_TRUE(file_util::ContentsEqual(output, expected));
  EXPECT_TRUE(file_util::Delete(output, false));
}

TEST(BlacklistIOTest, Combine) {
  // Testing data path.
  FilePath data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &data_dir);
  data_dir = data_dir.Append(FilePath::FromWStringHack(L"blacklist_samples"));

  FilePath input1 =
      data_dir.Append(FilePath::FromWStringHack(L"annoying_ads.pbl"));

  FilePath input2 =
    data_dir.Append(FilePath::FromWStringHack(L"block_flash.pbl"));

  FilePath input3 =
    data_dir.Append(FilePath::FromWStringHack(L"session_cookies.pbl"));

  BlacklistIO io;
  EXPECT_TRUE(io.Read(input1));
  EXPECT_TRUE(io.Read(input2));
  EXPECT_TRUE(io.Read(input3));

  const std::list<Blacklist::Entry*>& blacklist = io.blacklist();
  EXPECT_EQ(5U, blacklist.size());

  std::list<Blacklist::Entry*>::const_iterator i = blacklist.begin();
  EXPECT_EQ(Blacklist::kBlockAll, (*i)->attributes());
  EXPECT_EQ("annoying.ads.tv/@", (*i++)->pattern());
  EXPECT_EQ(Blacklist::kBlockAll, (*i)->attributes());
  EXPECT_EQ("@/annoying/120x600.jpg", (*i++)->pattern());
  EXPECT_EQ(Blacklist::kBlockAll, (*i)->attributes());
  EXPECT_EQ("@/annoying_ads/@", (*i++)->pattern());
  EXPECT_EQ(Blacklist::kBlockByType, (*i)->attributes());
  EXPECT_EQ("@", (*i++)->pattern());
  EXPECT_EQ(Blacklist::kDontPersistCookies, (*i)->attributes());
  EXPECT_EQ("@", (*i++)->pattern());

  const std::list<Blacklist::Provider*>& providers = io.providers();
  EXPECT_EQ(3U, providers.size());

  std::list<Blacklist::Provider*>::const_iterator j = providers.begin();
  EXPECT_EQ("AnnoyingAds", (*j)->name());
  EXPECT_EQ("http://www.ads.tv", (*j++)->url());
  EXPECT_EQ("BlockFlash", (*j)->name());
  EXPECT_EQ("http://www.google.com", (*j++)->url());
  EXPECT_EQ("SessionCookies", (*j)->name());
  EXPECT_EQ("http://www.google.com", (*j++)->url());

  FilePath output;
  PathService::Get(base::DIR_TEMP, &output);
  output = output.Append(FilePath::FromWStringHack(L"combine3.pbr"));

  FilePath expected =
      data_dir.Append(FilePath::FromWStringHack(L"combine3.pbr"));

  CHECK(io.Write(output));
  EXPECT_TRUE(file_util::ContentsEqual(output, expected));
  EXPECT_TRUE(file_util::Delete(output, false));
}
