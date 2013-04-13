// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class TestAccountChooserModel : public AccountChooserModel {
 public:
  TestAccountChooserModel(AccountChooserModelDelegate* delegate,
                          PrefService* prefs,
                          const AutofillMetrics& metric_logger)
      : AccountChooserModel(delegate, prefs, metric_logger,
                            DIALOG_TYPE_REQUEST_AUTOCOMPLETE) {}
  virtual ~TestAccountChooserModel() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAccountChooserModel);
};

class MockAccountChooserModelDelegate : public AccountChooserModelDelegate {
 public:
  MockAccountChooserModelDelegate() {}
  virtual ~MockAccountChooserModelDelegate() {}

  MOCK_METHOD0(AccountChoiceChanged, void());
};

class AccountChooserModelTest : public testing::Test {
 public:
  AccountChooserModelTest()
      : model_(&delegate_, profile_.GetPrefs(), metric_logger_) {}
  virtual ~AccountChooserModelTest() {}

  Profile* profile() { return &profile_; }
  MockAccountChooserModelDelegate* delegate() { return &delegate_; }
  TestAccountChooserModel* model() { return &model_; }
  const AutofillMetrics& metric_logger() { return metric_logger_; }

 private:
  TestingProfile profile_;
  MockAccountChooserModelDelegate delegate_;
  TestAccountChooserModel model_;
  AutofillMetrics metric_logger_;
};

}  // namespace

TEST_F(AccountChooserModelTest, ObeysPref) {
  // When "Pay without wallet" is false, use Wallet by default.
  {
    profile()->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet, false);
    TestAccountChooserModel model(delegate(), profile()->GetPrefs(),
                                  metric_logger());
    EXPECT_TRUE(model.WalletIsSelected());
  }
  // When the user chose to "Pay without wallet", use Autofill.
  {
    profile()->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet, true);
    TestAccountChooserModel model(delegate(), profile()->GetPrefs(),
                                  metric_logger());
    EXPECT_FALSE(model.WalletIsSelected());
  }
}

TEST_F(AccountChooserModelTest, IgnoresPrefChanges) {
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogPayWithoutWallet));
  EXPECT_TRUE(model()->WalletIsSelected());

  // Check that nothing changes while this dialog is running if a pref changes
  // (this could cause subtle bugs or annoyances if a user closes another
  // running dialog).
  profile()->GetPrefs()->SetBoolean(
      ::prefs::kAutofillDialogPayWithoutWallet, true);
  EXPECT_TRUE(model()->WalletIsSelected());
}

TEST_F(AccountChooserModelTest, HandlesError) {
  EXPECT_CALL(*delegate(), AccountChoiceChanged()).Times(1);

  ASSERT_TRUE(model()->WalletIsSelected());
  ASSERT_TRUE(model()->IsCommandIdEnabled(AccountChooserModel::kWalletItemId));

  model()->SetHadWalletError();
  EXPECT_FALSE(model()->WalletIsSelected());
  EXPECT_FALSE(model()->IsCommandIdEnabled(AccountChooserModel::kWalletItemId));
}

TEST_F(AccountChooserModelTest, HandlesSigninError) {
  EXPECT_CALL(*delegate(), AccountChoiceChanged()).Times(1);

  ASSERT_TRUE(model()->WalletIsSelected());
  ASSERT_TRUE(model()->IsCommandIdEnabled(AccountChooserModel::kWalletItemId));

  model()->SetHadWalletSigninError();
  EXPECT_FALSE(model()->WalletIsSelected());
  EXPECT_TRUE(model()->IsCommandIdEnabled(AccountChooserModel::kWalletItemId));
}

TEST_F(AccountChooserModelTest, RespectsUserChoice) {
  EXPECT_CALL(*delegate(), AccountChoiceChanged()).Times(2);

  model()->ExecuteCommand(AccountChooserModel::kAutofillItemId, 0);
  EXPECT_FALSE(model()->WalletIsSelected());

  model()->ExecuteCommand(AccountChooserModel::kWalletItemId, 0);
  EXPECT_TRUE(model()->WalletIsSelected());
}

}  // namespace autofill
