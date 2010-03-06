// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_address_view_controller_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_view_controller_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class AutoFillDialogObserverTester : public AutoFillDialogObserver {
 public:
  AutoFillDialogObserverTester()
    : hit_(false) {}
  virtual ~AutoFillDialogObserverTester() {}

  virtual void OnAutoFillDialogApply(
    std::vector<AutoFillProfile>* profiles,
    std::vector<CreditCard>* credit_cards) {
    hit_ = true;

    std::vector<AutoFillProfile>::iterator i;
    profiles_.clear();
    for (i = profiles->begin(); i != profiles->end(); ++i)
      profiles_.push_back(*i);

    std::vector<CreditCard>::iterator j;
    credit_cards_.clear();
    for (j = credit_cards->begin(); j != credit_cards->end(); ++j)
      credit_cards_.push_back(*j);
  }

  bool hit_;
  std::vector<AutoFillProfile> profiles_;
  std::vector<CreditCard> credit_cards_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillDialogObserverTester);
};

class AutoFillDialogControllerTest : public CocoaTest {
 public:
  AutoFillDialogControllerTest() {}

  void LoadDialog() {
    controller_ = [AutoFillDialogController
        controllerWithObserver:&observer_
              autoFillProfiles:profiles_
                   creditCards:credit_cards_
                       profile:helper_.profile()];
    [controller_ window];
  }

  BrowserTestHelper helper_;
  AutoFillDialogObserverTester observer_;
  AutoFillDialogController* controller_;    // weak reference
  std::vector<AutoFillProfile*> profiles_;  // weak references within vector
  std::vector<CreditCard*> credit_cards_;   // weak references within vector

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillDialogControllerTest);
};

TEST_F(AutoFillDialogControllerTest, SaveButtonInformsObserver) {
  LoadDialog();
  [controller_ save:nil];
  ASSERT_TRUE(observer_.hit_);
}

TEST_F(AutoFillDialogControllerTest, CancelButtonDoesNotInformObserver) {
  LoadDialog();
  [controller_ cancel:nil];
  ASSERT_FALSE(observer_.hit_);
}

TEST_F(AutoFillDialogControllerTest, NoEditsGiveBackOriginalProfile) {
  AutoFillProfile profile;
  profiles_.push_back(&profile);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.
  ASSERT_EQ(observer_.profiles_.size(), profiles_.size());

  // Contents should match.
  size_t i = 0;
  size_t count = profiles_.size();
  for (i = 0; i < count; i++)
    ASSERT_EQ(observer_.profiles_[i], *profiles_[i]);

  // Contents should not match a different profile.
  AutoFillProfile different_profile;
  different_profile.set_label(ASCIIToUTF16("different"));
  different_profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("joe"));
  for (i = 0; i < count; i++)
    ASSERT_NE(observer_.profiles_[i], different_profile);
}

TEST_F(AutoFillDialogControllerTest, NoEditsGiveBackOriginalCreditCard) {
  CreditCard credit_card(ASCIIToUTF16("myCC"), 345);
  credit_cards_.push_back(&credit_card);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.
  ASSERT_EQ(observer_.credit_cards_.size(), credit_cards_.size());

  // Contents should match.  With the exception of the |unique_id|.
  size_t i = 0;
  size_t count = credit_cards_.size();
  for (i = 0; i < count; i++) {
    credit_cards_[i]->set_unique_id(observer_.credit_cards_[i].unique_id());
    ASSERT_EQ(observer_.credit_cards_[i], *credit_cards_[i]);
  }

  // Contents should not match a different profile.
  CreditCard different_credit_card(ASCIIToUTF16("different"), 0);
  different_credit_card.SetInfo(
    AutoFillType(CREDIT_CARD_NUMBER), ASCIIToUTF16("1234"));
  for (i = 0; i < count; i++)
    ASSERT_NE(observer_.credit_cards_[i], different_credit_card);
}

TEST_F(AutoFillDialogControllerTest, AutoFillDataMutation) {
  AutoFillProfile profile(ASCIIToUTF16("Home"), 17);
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("David"));
  profile.SetInfo(AutoFillType(NAME_MIDDLE), ASCIIToUTF16("C"));
  profile.SetInfo(AutoFillType(NAME_LAST), ASCIIToUTF16("Holloway"));
  profile.SetInfo(AutoFillType(EMAIL_ADDRESS),
      ASCIIToUTF16("dhollowa@chromium.org"));
  profile.SetInfo(AutoFillType(COMPANY_NAME), ASCIIToUTF16("Google Inc."));
  profile.SetInfo(
      AutoFillType(ADDRESS_HOME_LINE1), ASCIIToUTF16("1122 Mountain View Road"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE2), ASCIIToUTF16("Suite #1"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_CITY),
      ASCIIToUTF16("Mountain View"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_STATE), ASCIIToUTF16("CA"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_ZIP), ASCIIToUTF16("94111"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("USA"));
  profile.SetInfo(AutoFillType(PHONE_HOME_COUNTRY_CODE), ASCIIToUTF16("01"));
  profile.SetInfo(AutoFillType(PHONE_HOME_CITY_CODE), ASCIIToUTF16("415"));
  profile.SetInfo(AutoFillType(PHONE_HOME_NUMBER), ASCIIToUTF16("5552258"));
  profile.SetInfo(AutoFillType(PHONE_FAX_COUNTRY_CODE), ASCIIToUTF16("02"));
  profile.SetInfo(AutoFillType(PHONE_FAX_CITY_CODE), ASCIIToUTF16("408"));
  profile.SetInfo(AutoFillType(PHONE_FAX_NUMBER), ASCIIToUTF16("7172258"));
  profiles_.push_back(&profile);

  LoadDialog();

  AutoFillAddressModel* am = [[[controller_ addressFormViewControllers]
      objectAtIndex:0] addressModel];
  EXPECT_TRUE([[am label] isEqualToString:@"Home"]);
  EXPECT_TRUE([[am firstName] isEqualToString:@"David"]);
  EXPECT_TRUE([[am middleName] isEqualToString:@"C"]);
  EXPECT_TRUE([[am lastName] isEqualToString:@"Holloway"]);
  EXPECT_TRUE([[am email] isEqualToString:@"dhollowa@chromium.org"]);
  EXPECT_TRUE([[am companyName] isEqualToString:@"Google Inc."]);
  EXPECT_TRUE([[am addressLine1] isEqualToString:@"1122 Mountain View Road"]);
  EXPECT_TRUE([[am addressLine2] isEqualToString:@"Suite #1"]);
  EXPECT_TRUE([[am city] isEqualToString:@"Mountain View"]);
  EXPECT_TRUE([[am state] isEqualToString:@"CA"]);
  EXPECT_TRUE([[am zip] isEqualToString:@"94111"]);
  EXPECT_TRUE([[am phoneCountryCode] isEqualToString:@"01"]);
  EXPECT_TRUE([[am phoneAreaCode] isEqualToString:@"415"]);
  EXPECT_TRUE([[am phoneNumber] isEqualToString:@"5552258"]);
  EXPECT_TRUE([[am faxCountryCode] isEqualToString:@"02"]);
  EXPECT_TRUE([[am faxAreaCode] isEqualToString:@"408"]);
  EXPECT_TRUE([[am faxNumber] isEqualToString:@"7172258"]);

  [controller_ save:nil];

  ASSERT_TRUE(observer_.hit_);
  ASSERT_TRUE(observer_.profiles_.size() == 1);

  profiles_[0]->set_unique_id(observer_.profiles_[0].unique_id());
  ASSERT_EQ(observer_.profiles_[0], *profiles_[0]);
}

TEST_F(AutoFillDialogControllerTest, CreditCardDataMutation) {
  CreditCard credit_card(ASCIIToUTF16("myCC"), 345);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("DCH"));
  credit_card.SetInfo(
    AutoFillType(CREDIT_CARD_NUMBER), ASCIIToUTF16("1234 5678 9101 1121"));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("01"));
  credit_card.SetInfo(
    AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), ASCIIToUTF16("2012"));
  credit_card.SetInfo(
    AutoFillType(CREDIT_CARD_VERIFICATION_CODE), ASCIIToUTF16("222"));
  credit_cards_.push_back(&credit_card);

  LoadDialog();

  AutoFillCreditCardModel* cm = [[[controller_ creditCardFormViewControllers]
      objectAtIndex:0] creditCardModel];
  EXPECT_TRUE([[cm label] isEqualToString:@"myCC"]);
  EXPECT_TRUE([[cm nameOnCard] isEqualToString:@"DCH"]);
  EXPECT_TRUE([[cm creditCardNumber] isEqualToString:@"1234 5678 9101 1121"]);
  EXPECT_TRUE([[cm expirationMonth] isEqualToString:@"01"]);
  EXPECT_TRUE([[cm expirationYear] isEqualToString:@"2012"]);
  EXPECT_TRUE([[cm cvcCode] isEqualToString:@"222"]);

  [controller_ save:nil];

  ASSERT_TRUE(observer_.hit_);
  ASSERT_TRUE(observer_.credit_cards_.size() == 1);

  credit_cards_[0]->set_unique_id(observer_.credit_cards_[0].unique_id());
  ASSERT_EQ(observer_.credit_cards_[0], *credit_cards_[0]);
}

TEST_F(AutoFillDialogControllerTest, TwoProfiles) {
  AutoFillProfile profile1(ASCIIToUTF16("One"), 1);
  profile1.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles_.push_back(&profile1);
  AutoFillProfile profile2(ASCIIToUTF16("Two"), 2);
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Bob"));
  profiles_.push_back(&profile2);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.  And should be 2.
  ASSERT_EQ(observer_.profiles_.size(), profiles_.size());
  ASSERT_EQ(observer_.profiles_.size(), 2UL);

  // Contents should match.  With the exception of the |unique_id|.
  for (size_t i = 0, count = profiles_.size(); i < count; i++) {
    profiles_[i]->set_unique_id(observer_.profiles_[i].unique_id());
    ASSERT_EQ(observer_.profiles_[i], *profiles_[i]);
  }
}

TEST_F(AutoFillDialogControllerTest, TwoCreditCards) {
  CreditCard credit_card1(ASCIIToUTF16("Visa"), 1);
  credit_card1.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards_.push_back(&credit_card1);
  CreditCard credit_card2(ASCIIToUTF16("Mastercard"), 2);
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Bob"));
  credit_cards_.push_back(&credit_card2);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match.  And should be 2.
  ASSERT_EQ(observer_.credit_cards_.size(), credit_cards_.size());
  ASSERT_EQ(observer_.credit_cards_.size(), 2UL);

  // Contents should match.  With the exception of the |unique_id|.
  for (size_t i = 0, count = credit_cards_.size(); i < count; i++) {
    credit_cards_[i]->set_unique_id(observer_.credit_cards_[i].unique_id());
    ASSERT_EQ(observer_.credit_cards_[i], *credit_cards_[i]);
  }
}

TEST_F(AutoFillDialogControllerTest, AddNewProfile) {
  AutoFillProfile profile(ASCIIToUTF16("One"), 1);
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles_.push_back(&profile);
  LoadDialog();
  [controller_ addNewAddress:nil];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 2.
  ASSERT_NE(observer_.profiles_.size(), profiles_.size());
  ASSERT_EQ(observer_.profiles_.size(), 2UL);

  // New address should match.
  AutoFillProfile new_profile(ASCIIToUTF16("New address"), 0);
  ASSERT_EQ(observer_.profiles_[1], new_profile);
}

TEST_F(AutoFillDialogControllerTest, AddNewCreditCard) {
  CreditCard credit_card(ASCIIToUTF16("Visa"), 1);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards_.push_back(&credit_card);
  LoadDialog();
  [controller_ addNewCreditCard:nil];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 2.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards_.size());
  ASSERT_EQ(observer_.credit_cards_.size(), 2UL);

  // New address should match.
  CreditCard new_credit_card(ASCIIToUTF16("New credit card"), 0);
  ASSERT_EQ(observer_.credit_cards_[1], new_credit_card);
}

TEST_F(AutoFillDialogControllerTest, DeleteProfile) {
  AutoFillProfile profile(ASCIIToUTF16("One"), 1);
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles_.push_back(&profile);
  LoadDialog();
  [controller_ deleteAddress:[[controller_ addressFormViewControllers]
      lastObject]];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 0.
  ASSERT_NE(observer_.profiles_.size(), profiles_.size());
  ASSERT_EQ(observer_.profiles_.size(), 0UL);
}

TEST_F(AutoFillDialogControllerTest, DeleteCreditCard) {
  CreditCard credit_card(ASCIIToUTF16("Visa"), 1);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards_.push_back(&credit_card);
  LoadDialog();
  [controller_ deleteCreditCard:[[controller_ creditCardFormViewControllers]
      lastObject]];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 0.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards_.size());
  ASSERT_EQ(observer_.credit_cards_.size(), 0UL);
}

TEST_F(AutoFillDialogControllerTest, TwoProfilesDeleteOne) {
  AutoFillProfile profile(ASCIIToUTF16("One"), 1);
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles_.push_back(&profile);
  AutoFillProfile profile2(ASCIIToUTF16("Two"), 2);
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Bob"));
  profiles_.push_back(&profile2);
  LoadDialog();
  [controller_ deleteAddress:[[controller_ addressFormViewControllers]
      lastObject]];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 0.
  ASSERT_NE(observer_.profiles_.size(), profiles_.size());
  ASSERT_EQ(observer_.profiles_.size(), 1UL);

  // First address should match.
  profiles_[0]->set_unique_id(observer_.profiles_[0].unique_id());
  ASSERT_EQ(observer_.profiles_[0], profile);
}

TEST_F(AutoFillDialogControllerTest, TwoCreditCardsDeleteOne) {
  CreditCard credit_card(ASCIIToUTF16("Visa"), 1);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards_.push_back(&credit_card);
  CreditCard credit_card2(ASCIIToUTF16("Mastercard"), 2);
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Bob"));
  credit_cards_.push_back(&credit_card2);
  LoadDialog();
  [controller_ deleteCreditCard:[[controller_ creditCardFormViewControllers]
      lastObject]];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should match be different.  New size should be 0.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards_.size());
  ASSERT_EQ(observer_.credit_cards_.size(), 1UL);

  // First credit card should match.
  credit_cards_[0]->set_unique_id(observer_.credit_cards_[0].unique_id());
  ASSERT_EQ(observer_.credit_cards_[0], credit_card);
}

TEST_F(AutoFillDialogControllerTest, AuxiliaryProfilesFalse) {
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_FALSE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

TEST_F(AutoFillDialogControllerTest, AuxiliaryProfilesTrue) {
  helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, true);
  LoadDialog();
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_TRUE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

TEST_F(AutoFillDialogControllerTest, AuxiliaryProfilesChanged) {
  helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  LoadDialog();
  [controller_ setAuxiliaryEnabled:YES];
  [controller_ save:nil];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_TRUE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

}
