// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class MockAccountChooserModelDelegate : public AccountChooserModelDelegate {
 public:
  MockAccountChooserModelDelegate() {}
  virtual ~MockAccountChooserModelDelegate() {}

  MOCK_METHOD0(AccountChoiceChanged, void());
};

class AccountChooserModelTest : public testing::Test {
 public:
  AccountChooserModelTest() : model_(&delegate_, profile_.GetPrefs()) {}
  virtual ~AccountChooserModelTest() {}

  Profile* profile() { return &profile_; }
  MockAccountChooserModelDelegate* delegate() { return &delegate_; }
  AccountChooserModel* model() { return &model_; }

 private:
  TestingProfile profile_;
  MockAccountChooserModelDelegate delegate_;
  AccountChooserModel model_;
};

}  // namespace

TEST_F(AccountChooserModelTest, ObeysPref) {
  EXPECT_CALL(*delegate(), AccountChoiceChanged()).Times(2);

  profile()->GetPrefs()->SetBoolean(
      prefs::kAutofillDialogPayWithoutWallet, false);
  EXPECT_TRUE(model()->WalletIsSelected());

  profile()->GetPrefs()->SetBoolean(
      prefs::kAutofillDialogPayWithoutWallet, true);
  EXPECT_FALSE(model()->WalletIsSelected());
}

TEST_F(AccountChooserModelTest, HandlesError) {
  EXPECT_CALL(*delegate(), AccountChoiceChanged()).Times(2);

  profile()->GetPrefs()->SetBoolean(
      prefs::kAutofillDialogPayWithoutWallet, false);
  EXPECT_TRUE(model()->WalletIsSelected());

  model()->SetHadWalletError();
  EXPECT_FALSE(model()->WalletIsSelected());
  EXPECT_FALSE(model()->IsCommandIdEnabled(0));
}

TEST_F(AccountChooserModelTest, HandlesSigninError) {
  EXPECT_CALL(*delegate(), AccountChoiceChanged()).Times(2);

  profile()->GetPrefs()->SetBoolean(
      prefs::kAutofillDialogPayWithoutWallet, false);
  EXPECT_TRUE(model()->WalletIsSelected());

  model()->SetHadWalletSigninError();
  EXPECT_FALSE(model()->WalletIsSelected());
  EXPECT_TRUE(model()->IsCommandIdEnabled(0));
}

TEST_F(AccountChooserModelTest, RespectsUserChoice) {
  EXPECT_CALL(*delegate(), AccountChoiceChanged()).Times(3);

  profile()->GetPrefs()->SetBoolean(
      prefs::kAutofillDialogPayWithoutWallet, false);
  EXPECT_TRUE(model()->WalletIsSelected());

  model()->ExecuteCommand(1, 0);
  EXPECT_FALSE(model()->WalletIsSelected());

  model()->ExecuteCommand(0, 0);
  EXPECT_TRUE(model()->WalletIsSelected());
}

}  // namespace autofill
