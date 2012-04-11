// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/protector/composite_settings_change.h"
#include "chrome/browser/protector/mock_setting_change.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::NiceMock;
using ::testing::Return;

namespace protector {

namespace {

string16 kTitle1 = ASCIIToUTF16("A");
string16 kTitle2 = ASCIIToUTF16("B");
string16 kTitle3 = ASCIIToUTF16("C");

};

class CompositeSettingsChangeTest : public testing::Test {
 protected:
  string16 GetApplyButtonText(const string16& apply_text) {
    return l10n_util::GetStringFUTF16(IDS_CHANGE_SETTING, apply_text);
  }

  TestingProfile profile_;
};

// Verify that MergeWith works and Apply/Discard/Timeout call same functions
// of all merged changes.
TEST_F(CompositeSettingsChangeTest, MergeWith) {
  // These instances will be owned by CompositeSettingsChange.
  MockSettingChange* change1 = new NiceMock<MockSettingChange>;
  MockSettingChange* change2 = new NiceMock<MockSettingChange>;
  MockSettingChange* change3 = new NiceMock<MockSettingChange>;

  EXPECT_CALL(*change1, MockInit(&profile_)).WillOnce(Return(true));
  ASSERT_TRUE(change1->Init(&profile_));
  EXPECT_CALL(*change2, MockInit(&profile_)).WillOnce(Return(true));
  ASSERT_TRUE(change2->Init(&profile_));
  EXPECT_CALL(*change3, MockInit(&profile_)).WillOnce(Return(true));
  ASSERT_TRUE(change3->Init(&profile_));

  // Compose 1st and 2nd changes together.
  scoped_ptr<CompositeSettingsChange> composite_change(
      change1->MergeWith(change2));

  EXPECT_TRUE(composite_change->Contains(change1));
  EXPECT_TRUE(composite_change->Contains(change2));
  EXPECT_FALSE(composite_change->Contains(change3));
  EXPECT_TRUE(composite_change->Contains(composite_change.get()));

  // Add third change.
  EXPECT_EQ(composite_change.get(), composite_change->MergeWith(change3));

  EXPECT_TRUE(composite_change->Contains(change1));
  EXPECT_TRUE(composite_change->Contains(change2));
  EXPECT_TRUE(composite_change->Contains(change3));
  EXPECT_TRUE(composite_change->Contains(composite_change.get()));

  // Apply/Discard/Timeout calls should be proxied to inner change instances:
  EXPECT_CALL(*change1, Apply(NULL));
  EXPECT_CALL(*change2, Apply(NULL));
  EXPECT_CALL(*change3, Apply(NULL));
  composite_change->Apply(NULL);

  EXPECT_CALL(*change1, Discard(NULL));
  EXPECT_CALL(*change2, Discard(NULL));
  EXPECT_CALL(*change3, Discard(NULL));
  composite_change->Discard(NULL);

  EXPECT_CALL(*change1, Timeout());
  EXPECT_CALL(*change2, Timeout());
  EXPECT_CALL(*change3, Timeout());
  composite_change->Timeout();
}

TEST_F(CompositeSettingsChangeTest, ApplyButtonText) {
  // These instances will be owned by CompositeSettingsChange.
  MockSettingChange* change1 = new NiceMock<MockSettingChange>;
  MockSettingChange* change2 = new NiceMock<MockSettingChange>;
  MockSettingChange* change3 = new NiceMock<MockSettingChange>;

  EXPECT_CALL(*change1, MockInit(&profile_)).WillOnce(Return(true));
  ASSERT_TRUE(change1->Init(&profile_));
  EXPECT_CALL(*change2, MockInit(&profile_)).WillOnce(Return(true));
  ASSERT_TRUE(change2->Init(&profile_));
  EXPECT_CALL(*change3, MockInit(&profile_)).WillOnce(Return(true));
  ASSERT_TRUE(change3->Init(&profile_));

  // |change1| has higher priority.
  EXPECT_CALL(*change1, GetApplyDisplayName()).WillOnce(
      Return(BaseSettingChange::DisplayName(10, kTitle1)));
  EXPECT_CALL(*change2, GetApplyDisplayName()).WillOnce(
      Return(BaseSettingChange::DisplayName(5, kTitle2)));

  scoped_ptr<CompositeSettingsChange> composite_change(
      change1->MergeWith(change2));

  EXPECT_EQ(GetApplyButtonText(kTitle1),
            composite_change->GetApplyButtonText());

  // |change3| has the highest priority now.
  EXPECT_CALL(*change3, GetApplyDisplayName()).WillOnce(
      Return(BaseSettingChange::DisplayName(15, kTitle3)));
  EXPECT_EQ(composite_change.get(), composite_change->MergeWith(change3));

  EXPECT_EQ(GetApplyButtonText(kTitle3),
            composite_change->GetApplyButtonText());

  GURL url1("example.com");
  EXPECT_CALL(*change1, GetNewSettingURL()).WillOnce(Return(url1));
  EXPECT_EQ(url1, composite_change->GetNewSettingURL());
}

}  // namespace protector
