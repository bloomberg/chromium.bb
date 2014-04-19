// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/account_chooser_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

namespace {

class TestAccountChooserModel : public AccountChooserModel {
 public:
  TestAccountChooserModel(AccountChooserModelDelegate* delegate,
                          Profile* profile,
                          bool disable_wallet,
                          const AutofillMetrics& metric_logger)
      : AccountChooserModel(delegate, profile, disable_wallet, metric_logger) {}
  virtual ~TestAccountChooserModel() {}

  using AccountChooserModel::kWalletAccountsStartId;
  using AccountChooserModel::kWalletAddAccountId;
  using AccountChooserModel::kAutofillItemId;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAccountChooserModel);
};

class MockAccountChooserModelDelegate : public AccountChooserModelDelegate {
 public:
  MockAccountChooserModelDelegate() {}
  virtual ~MockAccountChooserModelDelegate() {}

  MOCK_METHOD0(AccountChoiceChanged, void());
  MOCK_METHOD0(AddAccount, void());
  MOCK_METHOD0(UpdateAccountChooserView, void());
};

class AccountChooserModelTest : public testing::Test {
 public:
  AccountChooserModelTest()
      : model_(&delegate_, &profile_, false, metric_logger_) {}
  virtual ~AccountChooserModelTest() {}

  TestingProfile* profile() { return &profile_; }
  MockAccountChooserModelDelegate* delegate() { return &delegate_; }
  TestAccountChooserModel* model() { return &model_; }
  const AutofillMetrics& metric_logger() { return metric_logger_; }

 private:
  TestingProfile profile_;
  testing::NiceMock<MockAccountChooserModelDelegate> delegate_;
  TestAccountChooserModel model_;
  AutofillMetrics metric_logger_;
};

}  // namespace

TEST_F(AccountChooserModelTest, ObeysPref) {
  // When "Pay without wallet" is false, use Wallet by default.
  {
    profile()->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet, false);
    TestAccountChooserModel model(delegate(),
                                  profile(),
                                  false,
                                  metric_logger());
    EXPECT_TRUE(model.WalletIsSelected());
  }
  // When the user chose to "Pay without wallet", use Autofill.
  {
    profile()->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet, true);
    TestAccountChooserModel model(delegate(),
                                  profile(),
                                  false,
                                  metric_logger());
    EXPECT_FALSE(model.WalletIsSelected());
  }
  // When the |disable_wallet| argument is true, use Autofill regardless
  // of the pref.
  {
    profile()->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet, false);
    TestAccountChooserModel model(delegate(), profile(), true, metric_logger());
    EXPECT_FALSE(model.WalletIsSelected());
  }
  // In incognito, use local data regardless of the pref.
  {
    TestingProfile::Builder builder;
    builder.SetIncognito();
    scoped_ptr<TestingProfile> incognito = builder.Build();
    incognito->SetOriginalProfile(profile());
    profile()->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet, false);
    incognito->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet, false);

    TestAccountChooserModel model(delegate(),
                                  incognito.get(),
                                  false,
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
  EXPECT_CALL(*delegate(), UpdateAccountChooserView()).Times(1);

  ASSERT_TRUE(model()->WalletIsSelected());
  ASSERT_TRUE(model()->IsCommandIdEnabled(
      TestAccountChooserModel::kWalletAccountsStartId));

  model()->SetHadWalletError();
  EXPECT_FALSE(model()->WalletIsSelected());
  EXPECT_FALSE(model()->IsCommandIdEnabled(
      TestAccountChooserModel::kWalletAccountsStartId));
}

TEST_F(AccountChooserModelTest, HandlesSigninError) {
  EXPECT_CALL(*delegate(), AccountChoiceChanged()).Times(1);
  EXPECT_CALL(*delegate(), UpdateAccountChooserView()).Times(2);

  // 0. "Unknown" wallet account, we don't know if the user is signed-in yet.
  ASSERT_TRUE(model()->WalletIsSelected());
  ASSERT_TRUE(model()->IsCommandIdEnabled(
      TestAccountChooserModel::kWalletAccountsStartId));
  ASSERT_TRUE(model()->WalletIsSelected());
  ASSERT_FALSE(model()->HasAccountsToChoose());
  ASSERT_EQ(3, model()->GetItemCount());
  EXPECT_EQ(base::string16(), model()->GetActiveWalletAccountName());

  // 1. "Known" wallet account (e.g. after active/passive/automatic sign-in).
  // Calls UpdateAccountChooserView.
  std::vector<std::string> accounts;
  accounts.push_back("john.doe@gmail.com");
  model()->SetWalletAccounts(accounts, 0U);
  ASSERT_TRUE(model()->WalletIsSelected());
  ASSERT_TRUE(model()->IsCommandIdEnabled(
      TestAccountChooserModel::kWalletAccountsStartId));
  ASSERT_TRUE(model()->WalletIsSelected());
  ASSERT_TRUE(model()->HasAccountsToChoose());
  EXPECT_EQ(3, model()->GetItemCount());
  EXPECT_EQ(ASCIIToUTF16(accounts[0]), model()->GetActiveWalletAccountName());

  // 2. Sign-in failure.
  // Autofill data should be selected and be the only valid choice.
  // Calls UpdateAccountChooserView.
  // Calls AccountChoiceChanged.
  model()->SetHadWalletSigninError();
  EXPECT_FALSE(model()->WalletIsSelected());
  EXPECT_TRUE(model()->IsCommandIdEnabled(
      TestAccountChooserModel::kWalletAccountsStartId));
  EXPECT_FALSE(model()->WalletIsSelected());
  EXPECT_FALSE(model()->HasAccountsToChoose());
  EXPECT_EQ(2, model()->GetItemCount());
  EXPECT_EQ(base::string16(), model()->GetActiveWalletAccountName());
}

TEST_F(AccountChooserModelTest, RespectsUserChoice) {
  EXPECT_CALL(*delegate(), AccountChoiceChanged()).Times(2);

  model()->ExecuteCommand(TestAccountChooserModel::kAutofillItemId, 0);
  EXPECT_FALSE(model()->WalletIsSelected());

  EXPECT_CALL(*delegate(), AddAccount());
  model()->ExecuteCommand(TestAccountChooserModel::kWalletAddAccountId, 0);
  EXPECT_FALSE(model()->WalletIsSelected());

  model()->ExecuteCommand(TestAccountChooserModel::kWalletAccountsStartId, 0);
  EXPECT_TRUE(model()->WalletIsSelected());
}

TEST_F(AccountChooserModelTest, HandlesMultipleAccounts) {
  EXPECT_FALSE(model()->HasAccountsToChoose());

  std::vector<std::string> accounts;
  accounts.push_back("john.doe@gmail.com");
  accounts.push_back("jane.smith@gmail.com");
  model()->SetWalletAccounts(accounts, 0U);
  EXPECT_TRUE(model()->HasAccountsToChoose());
  EXPECT_TRUE(model()->WalletIsSelected());
  ASSERT_EQ(4, model()->GetItemCount());
  EXPECT_TRUE(model()->IsCommandIdEnabled(
      TestAccountChooserModel::kWalletAccountsStartId));
  EXPECT_TRUE(model()->IsCommandIdEnabled(
      TestAccountChooserModel::kWalletAccountsStartId + 1));
  EXPECT_EQ(ASCIIToUTF16(accounts[0]), model()->GetActiveWalletAccountName());
  model()->SetWalletAccounts(accounts, 1U);
  EXPECT_EQ(ASCIIToUTF16(accounts[1]), model()->GetActiveWalletAccountName());

  model()->ExecuteCommand(TestAccountChooserModel::kWalletAccountsStartId, 0);
  EXPECT_EQ(ASCIIToUTF16(accounts[0]), model()->GetActiveWalletAccountName());
  model()->ExecuteCommand(TestAccountChooserModel::kWalletAccountsStartId + 1,
                          0);
  EXPECT_EQ(ASCIIToUTF16(accounts[1]), model()->GetActiveWalletAccountName());

  // Setting the wallet accounts forces the switch to wallet.
  model()->ExecuteCommand(TestAccountChooserModel::kAutofillItemId, 0);
  EXPECT_FALSE(model()->WalletIsSelected());
  model()->SetWalletAccounts(accounts, 1U);
  EXPECT_TRUE(model()->WalletIsSelected());
}

}  // namespace autofill
