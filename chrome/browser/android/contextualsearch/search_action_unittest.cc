// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/search_action.h"

#include "base/gtest_prod_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::string16;
using base::UTF8ToUTF16;
using std::pair;

// Tests parts of the SearchAction class.
// This is part of the 2016-refactoring (crbug.com/624609,
// go/cs-refactoring-2016).
class SearchActionTest : public testing::Test {
 public:
  SearchActionTest() {}
  ~SearchActionTest() override {}

  // The class under test.
  std::unique_ptr<SearchAction> search_action_;

 protected:
  void SetUp() override { search_action_.reset(new SearchAction()); }
  void TearDown() override {}

  // Helper to set the context to the given sample and focus on |focus|.
  void SetContext(std::string sample, std::string focus);
};

void SearchActionTest::SetContext(std::string sample, std::string focus) {
  size_t offset = sample.find(focus);
  ASSERT_NE(offset, std::string::npos);
  search_action_->SetContext(sample, offset, offset + focus.length());
}

TEST_F(SearchActionTest, IsValidCharacterTest) {
  EXPECT_TRUE(search_action_->IsValidCharacter('a'));
  EXPECT_TRUE(search_action_->IsValidCharacter('A'));
  EXPECT_TRUE(search_action_->IsValidCharacter('0'));

  EXPECT_FALSE(search_action_->IsValidCharacter(','));
  EXPECT_FALSE(search_action_->IsValidCharacter(' '));
  EXPECT_FALSE(search_action_->IsValidCharacter('-'));
}

TEST_F(SearchActionTest, FindFocusedWordTest) {
  // Test finding "word" within this sample string.
  std::string sample = "Sample word, text";

  // Any range inside the word but before the end should return the word.
  search_action_->SetContext(sample, 7, 7);
  EXPECT_EQ("word", search_action_->FindFocusedWord());
  search_action_->SetContext(sample, 10, 10);
  EXPECT_EQ("word", search_action_->FindFocusedWord());
  search_action_->SetContext(sample, 7, 11);
  EXPECT_EQ("word", search_action_->FindFocusedWord());

  // A range just past the word returns an empty string.
  search_action_->SetContext(sample, 11, 11);
  EXPECT_EQ("", search_action_->FindFocusedWord());
}

TEST_F(SearchActionTest, SampleSurroundingsTest) {
  std::string focus = "focus";
  std::string sample = "987654321focus123456789";

  // Sample big enough to include both ends.
  SetContext(sample, focus);
  EXPECT_EQ(sample, search_action_->GetSampleText(100));

  // Must trim both ends, trimming 6 off each end.
  EXPECT_EQ("321focus123", search_action_->GetSampleText(12));

  // With focus near the beginning, extra is shifted to the end.
  SetContext("321focus123456789", focus);
  EXPECT_EQ("321focus12345", search_action_->GetSampleText(13));

  // With focus near the end, extra is shifted to the beginning.
  SetContext("987654321focus123", focus);
  EXPECT_EQ("54321focus123", search_action_->GetSampleText(13));

  // Requesting less than the focus.
  SetContext("focus", focus);
  EXPECT_EQ("c", search_action_->GetSampleText(1));
}
