// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/title_prefix_matcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const string16 kFoofooAbcdef(ASCIIToUTF16("Foofoo abcdef"));
const string16 kFoofooAbcdeg(ASCIIToUTF16("Foofoo abcdeg"));
const string16 kFooAbcdef(ASCIIToUTF16("Foo abcdef"));
const string16 kFooAbcdeg(ASCIIToUTF16("Foo abcdeg"));
const string16 kBarAbcDef(ASCIIToUTF16("Bar abc def"));
const string16 kBarAbcDeg(ASCIIToUTF16("Bar abc deg"));
const string16 kBarAbdDef(ASCIIToUTF16("Bar abd def"));
const string16 kBar(ASCIIToUTF16("Bar"));
const string16 kFoo(ASCIIToUTF16("Foo"));

}

TEST(TitlePrefixMatcherTest, BasicTests) {
  std::vector<TitlePrefixMatcher::TitleInfo> tab_title_infos;
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdef, 0));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdeg, 1));

  TitlePrefixMatcher::CalculatePrefixLengths(&tab_title_infos);
  EXPECT_EQ(0, tab_title_infos[0].caller_value);
  EXPECT_EQ(7U, tab_title_infos[0].prefix_length);

  EXPECT_EQ(1, tab_title_infos[1].caller_value);
  EXPECT_EQ(7U, tab_title_infos[1].prefix_length);

  tab_title_infos.clear();
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdef, 0));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdeg, 1));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdef, 2));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdeg, 3));

  TitlePrefixMatcher::CalculatePrefixLengths(&tab_title_infos);
  EXPECT_EQ(0, tab_title_infos[0].caller_value);
  EXPECT_EQ(7U, tab_title_infos[0].prefix_length);

  EXPECT_EQ(1, tab_title_infos[1].caller_value);
  EXPECT_EQ(7U, tab_title_infos[1].prefix_length);

  EXPECT_EQ(2, tab_title_infos[2].caller_value);
  EXPECT_EQ(4U, tab_title_infos[2].prefix_length);

  EXPECT_EQ(3, tab_title_infos[3].caller_value);
  EXPECT_EQ(4U, tab_title_infos[3].prefix_length);
}

TEST(TitlePrefixMatcherTest, Duplicates) {
  std::vector<TitlePrefixMatcher::TitleInfo> tab_title_infos;
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdef, 0));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdeg, 1));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdef, 2));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdeg, 3));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdef, 4));

  TitlePrefixMatcher::CalculatePrefixLengths(&tab_title_infos);
  EXPECT_EQ(0, tab_title_infos[0].caller_value);
  EXPECT_EQ(0U, tab_title_infos[0].prefix_length);

  EXPECT_EQ(1, tab_title_infos[1].caller_value);
  EXPECT_EQ(0U, tab_title_infos[1].prefix_length);

  EXPECT_EQ(2, tab_title_infos[2].caller_value);
  EXPECT_EQ(4U, tab_title_infos[2].prefix_length);

  EXPECT_EQ(3, tab_title_infos[3].caller_value);
  EXPECT_EQ(4U, tab_title_infos[3].prefix_length);

  EXPECT_EQ(4, tab_title_infos[4].caller_value);
  EXPECT_EQ(0U, tab_title_infos[4].prefix_length);
}

TEST(TitlePrefixMatcherTest, MultiplePrefixes) {
  std::vector<TitlePrefixMatcher::TitleInfo> tab_title_infos;
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdef, 0));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdeg, 1));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBarAbcDef, 2));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBarAbcDeg, 3));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBarAbdDef, 4));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBar, 5));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoo, 6));

  TitlePrefixMatcher::CalculatePrefixLengths(&tab_title_infos);
  EXPECT_EQ(0, tab_title_infos[0].caller_value);
  EXPECT_EQ(4U, tab_title_infos[0].prefix_length);

  EXPECT_EQ(1, tab_title_infos[1].caller_value);
  EXPECT_EQ(4U, tab_title_infos[1].prefix_length);

  EXPECT_EQ(2, tab_title_infos[2].caller_value);
  EXPECT_EQ(8U, tab_title_infos[2].prefix_length);

  EXPECT_EQ(3, tab_title_infos[3].caller_value);
  EXPECT_EQ(8U, tab_title_infos[3].prefix_length);

  EXPECT_EQ(4, tab_title_infos[4].caller_value);
  EXPECT_EQ(4U, tab_title_infos[4].prefix_length);

  EXPECT_EQ(5, tab_title_infos[5].caller_value);
  EXPECT_EQ(0U, tab_title_infos[5].prefix_length);

  EXPECT_EQ(6, tab_title_infos[6].caller_value);
  EXPECT_EQ(0U, tab_title_infos[6].prefix_length);
}
