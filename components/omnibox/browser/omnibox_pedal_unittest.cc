// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal.h"

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_pedal_implementations.h"
#include "components/omnibox/browser/omnibox_pedal_provider.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class OmniboxPedalTest : public testing::Test {
 protected:
  OmniboxPedalTest() {}
};

TEST_F(OmniboxPedalTest, SynonymGroupRespectsSingle) {
  {
    // Test |single| = true:
    // Only the last instance of first found representative should be removed.
    auto group = OmniboxPedal::SynonymGroup(true, true);
    group.AddSynonym(base::ASCIIToUTF16("hello"));
    group.AddSynonym(base::ASCIIToUTF16("hi"));
    base::string16 text = base::ASCIIToUTF16("hello hi world hello hi");
    const bool found = group.EraseMatchesIn(text);
    EXPECT_TRUE(found);
    EXPECT_EQ(text, base::ASCIIToUTF16("  hi world hello hi"));
  }
  {
    // Test |single| = false:
    // All matches should be removed.
    auto group = OmniboxPedal::SynonymGroup(true, false);
    group.AddSynonym(base::ASCIIToUTF16("hello"));
    group.AddSynonym(base::ASCIIToUTF16("hi"));
    base::string16 text = base::ASCIIToUTF16("hello hi world hello hi");
    const bool found = group.EraseMatchesIn(text);
    EXPECT_TRUE(found);
    EXPECT_EQ(text, base::ASCIIToUTF16("   world   "));
  }
}

TEST_F(OmniboxPedalTest, SynonymGroupRespectsBoundariesAndWhitespace) {
  auto group = OmniboxPedal::SynonymGroup(true, false);
  group.AddSynonym(base::ASCIIToUTF16("aaa"));
  group.AddSynonym(base::ASCIIToUTF16("bb"));
  group.AddSynonym(base::ASCIIToUTF16("c"));
  {
    base::string16 text = base::ASCIIToUTF16("aaaa bbb cc aaabbc bbc cbb");
    const bool found = group.EraseMatchesIn(text);
    EXPECT_FALSE(found);
    EXPECT_EQ(text, base::ASCIIToUTF16("aaaa bbb cc aaabbc bbc cbb"));
  }
  {
    base::string16 text = base::ASCIIToUTF16("aaa bb c aaa bb c bb c c bb");
    const bool found = group.EraseMatchesIn(text);
    EXPECT_TRUE(found);
    EXPECT_EQ(text, base::ASCIIToUTF16("           "));
  }
}

TEST_F(OmniboxPedalTest, SynonymGroupsDriveConceptMatches) {
  OmniboxPedal test_pedal(
      OmniboxPedal::LabelStrings(
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT_SHORT,
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS),
      GURL(),
      {
          "test trigger phrase",
      });
  OmniboxPedal::SynonymGroup optional(false, true);
  optional.AddSynonym(base::ASCIIToUTF16("optional"));
  OmniboxPedal::SynonymGroup required_a(true, true);
  required_a.AddSynonym(base::ASCIIToUTF16("required_a"));
  OmniboxPedal::SynonymGroup required_b(true, true);
  required_b.AddSynonym(base::ASCIIToUTF16("required_b"));
  test_pedal.AddSynonymGroup(optional);
  test_pedal.AddSynonymGroup(required_a);
  test_pedal.AddSynonymGroup(required_b);
  const auto is_concept_match = [&](const char* text) {
    return test_pedal.IsConceptMatch(base::ASCIIToUTF16(text));
  };

  // As long as required synonym groups are present, order shouldn't matter.
  EXPECT_TRUE(is_concept_match("required_a required_b"));
  EXPECT_TRUE(is_concept_match("required_b required_a"));

  // Optional groups may be added without stopping trigger.
  EXPECT_TRUE(is_concept_match("required_a required_b optional"));
  EXPECT_TRUE(is_concept_match("required_a optional required_b"));
  EXPECT_TRUE(is_concept_match("optional required_b required_a"));

  // Any required group's absence will stop trigger.
  EXPECT_FALSE(is_concept_match("required_a optional"));
  EXPECT_FALSE(is_concept_match("nonsense"));
  EXPECT_FALSE(is_concept_match("nonsense optional"));

  // Presence of extra text will stop trigger even with all required present.
  EXPECT_FALSE(is_concept_match("required_a required_b nonsense optional"));
  EXPECT_FALSE(is_concept_match("required_b required_a nonsense"));
}
