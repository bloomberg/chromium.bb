// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/title_prefix_matcher.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const GURL kUrlA1("http://a.b.c/123");
const GURL kUrlA2("http://a.b.c/alphabits");
const GURL kUrlB1("http://www.here.com/here/and/there");
const GURL kUrlB2("http://www.here.com/elsewhere");
const GURL kUrlC1("http://www.here.ca/far/far/away");
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
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdef,
                                                          kUrlA1, 0));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdeg,
                                                          kUrlA1, 1));

  TitlePrefixMatcher::CalculatePrefixLengths(&tab_title_infos);
  EXPECT_EQ(0, tab_title_infos[0].caller_value);
  EXPECT_EQ(7U, tab_title_infos[0].prefix_length);

  EXPECT_EQ(1, tab_title_infos[1].caller_value);
  EXPECT_EQ(7U, tab_title_infos[1].prefix_length);

  tab_title_infos.clear();
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdef,
                                                          kUrlA1, 0));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdeg,
                                                          kUrlA1, 1));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdef,
                                                          kUrlA1, 2));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdeg,
                                                          kUrlA1, 3));

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
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdef,
                                                          kUrlA1, 0));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdeg,
                                                          kUrlA1, 1));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdef,
                                                          kUrlA1, 2));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdeg,
                                                          kUrlA1, 3));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdef,
                                                          kUrlA1, 4));

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
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdef,
                                                          kUrlA1, 0));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdeg,
                                                          kUrlA1, 1));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBarAbcDef,
                                                          kUrlA1, 2));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBarAbcDeg,
                                                          kUrlA1, 3));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBarAbdDef,
                                                          kUrlA1, 4));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBar,
                                                          kUrlA1, 5));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoo,
                                                          kUrlA1, 6));

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

TEST(TitlePrefixMatcherTest, DifferentHosts) {
  std::vector<TitlePrefixMatcher::TitleInfo> tab_title_infos;
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdef,
                                                          kUrlA1, 0));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFoofooAbcdeg,
                                                          kUrlA2, 1));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdef,
                                                          kUrlB1, 2));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBarAbdDef,
                                                          kUrlC1, 3));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBarAbcDef,
                                                          kUrlA1, 4));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kBarAbcDeg,
                                                          kUrlA2, 5));
  tab_title_infos.push_back(TitlePrefixMatcher::TitleInfo(&kFooAbcdeg,
                                                          kUrlB2, 6));

  TitlePrefixMatcher::CalculatePrefixLengths(&tab_title_infos);
  EXPECT_EQ(0, tab_title_infos[0].caller_value);
  EXPECT_EQ(7U, tab_title_infos[0].prefix_length);

  EXPECT_EQ(1, tab_title_infos[1].caller_value);
  EXPECT_EQ(7U, tab_title_infos[1].prefix_length);

  EXPECT_EQ(2, tab_title_infos[2].caller_value);
  EXPECT_EQ(4U, tab_title_infos[2].prefix_length);

  EXPECT_EQ(3, tab_title_infos[3].caller_value);
  EXPECT_EQ(0U, tab_title_infos[3].prefix_length);

  EXPECT_EQ(4, tab_title_infos[4].caller_value);
  EXPECT_EQ(8U, tab_title_infos[4].prefix_length);

  EXPECT_EQ(5, tab_title_infos[5].caller_value);
  EXPECT_EQ(8U, tab_title_infos[5].prefix_length);

  EXPECT_EQ(6, tab_title_infos[6].caller_value);
  EXPECT_EQ(4U, tab_title_infos[6].prefix_length);
}
