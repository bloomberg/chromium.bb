// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_address_sheet_controller_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#import "chrome/browser/autofill/autofill_credit_card_sheet_controller_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Simulated delay (in milliseconds) for web data loading.
const float kWebDataLoadDelayMilliseconds = 10.0;

// Mock PersonalDataManager that gives back canned profiles and credit cards
// as well as simulating delayed loading of web data using the
// |PersonalDataManager::Observer| interface.
class PersonalDataManagerMock : public PersonalDataManager {
 public:
  PersonalDataManagerMock()
      : observer_(NULL),
        test_data_is_loaded_(true) {}
  virtual ~PersonalDataManagerMock() {}

  virtual const std::vector<AutoFillProfile*>& web_profiles() {
    return test_profiles_;
  }
  virtual const std::vector<CreditCard*>& credit_cards() {
    return test_credit_cards_;
  }
  virtual bool IsDataLoaded() const { return test_data_is_loaded_; }
  virtual void SetObserver(PersonalDataManager::Observer* observer) {
    DCHECK(observer);
    observer_ = observer;

    // This delay allows the UI loop to run and display intermediate results
    // while the data is loading.  When notified that the data is available the
    // UI updates with the new data.  10ms is a nice short amount of time to
    // let the UI thread update but does not slow down the tests too much.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        new MessageLoop::QuitTask,
        kWebDataLoadDelayMilliseconds);
    MessageLoop::current()->Run();
    observer_->OnPersonalDataLoaded();
  }
  virtual void RemoveObserver(PersonalDataManager::Observer* observer) {
    observer_ = NULL;
  }

  std::vector<AutoFillProfile*> test_profiles_;
  std::vector<CreditCard*> test_credit_cards_;
  PersonalDataManager::Observer* observer_;
  bool test_data_is_loaded_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerMock);
};

// Mock profile that gives back our own mock |PersonalDataManager|.
class ProfileMock : public TestingProfile {
 public:
  ProfileMock() {
    test_manager_ = new PersonalDataManagerMock;
  }
  virtual ~ProfileMock() {}

  virtual PersonalDataManager* GetPersonalDataManager() {
    return test_manager_.get();
  }

  scoped_refptr<PersonalDataManagerMock> test_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileMock);
};

// Mock browser that gives back our own |BrowserMock| instance as the profile.
class BrowserMock : public BrowserTestHelper {
 public:
  BrowserMock() {
    test_profile_.reset(new ProfileMock);
  }
  virtual ~BrowserMock() {}

  // Override of |BrowserTestHelper::profile()|.
  virtual TestingProfile* profile() const {
    return test_profile_.get();
  }

  scoped_ptr<ProfileMock> test_profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserMock);
};

// Mock observer for the AutoFill settings dialog.
class AutoFillDialogObserverMock : public AutoFillDialogObserver {
 public:
  AutoFillDialogObserverMock()
    : hit_(false) {}
  virtual ~AutoFillDialogObserverMock() {}

  virtual void OnAutoFillDialogApply(std::vector<AutoFillProfile>* profiles,
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
  DISALLOW_COPY_AND_ASSIGN(AutoFillDialogObserverMock);
};

// Test fixture for setting up and tearing down our dialog controller under
// test.  Also provides helper methods to access the source profiles and
// credit card information stored in mock |PersonalDataManager|.
class AutoFillDialogControllerTest : public CocoaTest {
 public:
  AutoFillDialogControllerTest()
      : controller_(nil),
        imported_profile_(NULL),
        imported_credit_card_(NULL) {
  }

  void LoadDialog() {
    controller_ = [AutoFillDialogController
        controllerWithObserver:&observer_ profile:helper_.profile()];
    [controller_ runModelessDialog];
  }

  std::vector<AutoFillProfile*>& profiles() {
    return helper_.test_profile_->test_manager_->test_profiles_;
  }
  std::vector<CreditCard*>& credit_cards() {
    return helper_.test_profile_->test_manager_->test_credit_cards_;
  }

  BrowserMock helper_;
  AutoFillDialogObserverMock observer_;
  AutoFillDialogController* controller_;  // weak reference
  AutoFillProfile* imported_profile_;  // weak reference
  CreditCard* imported_credit_card_;  // weak reference

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillDialogControllerTest);
};

TEST_F(AutoFillDialogControllerTest, CloseButtonDoesNotInformObserver) {
  LoadDialog();
  [controller_ closeDialog];
  ASSERT_FALSE(observer_.hit_);
}

TEST_F(AutoFillDialogControllerTest, NoEditsDoNotChangeObserverProfiles) {
  AutoFillProfile profile;
  profiles().push_back(&profile);
  LoadDialog();
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Observer should not have profiles.
  ASSERT_EQ(0UL, observer_.profiles_.size());
}

TEST_F(AutoFillDialogControllerTest, NoEditsDoNotChangeObserverCreditCards) {
  CreditCard credit_card;
  credit_cards().push_back(&credit_card);
  LoadDialog();
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Observer should not have credit cards.
  ASSERT_EQ(0UL, observer_.credit_cards_.size());
}

TEST_F(AutoFillDialogControllerTest, AutoFillDataMutation) {
  AutoFillProfile profile;
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("John"));
  profile.SetInfo(AutoFillType(NAME_MIDDLE), ASCIIToUTF16("C"));
  profile.SetInfo(AutoFillType(NAME_LAST), ASCIIToUTF16("Smith"));
  profile.SetInfo(AutoFillType(EMAIL_ADDRESS),
                  ASCIIToUTF16("john@chromium.org"));
  profile.SetInfo(AutoFillType(COMPANY_NAME), ASCIIToUTF16("Google Inc."));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE1),
                  ASCIIToUTF16("1122 Mountain View Road"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE2), ASCIIToUTF16("Suite #1"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_CITY),
                  ASCIIToUTF16("Mountain View"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_STATE), ASCIIToUTF16("CA"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_ZIP), ASCIIToUTF16("94111"));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("USA"));
  profile.SetInfo(AutoFillType(PHONE_HOME_WHOLE_NUMBER),
                  ASCIIToUTF16("014155552258"));
  profile.SetInfo(AutoFillType(PHONE_FAX_WHOLE_NUMBER),
                  ASCIIToUTF16("024087172258"));
  profiles().push_back(&profile);

  LoadDialog();
  [controller_ selectAddressAtIndex:0];
  [controller_ editSelection:nil];

  AutoFillAddressSheetController* sheet = [controller_ addressSheetController];
  ASSERT_TRUE(sheet != nil);
  AutoFillAddressModel* am = [sheet addressModel];
  EXPECT_TRUE([[am fullName] isEqualToString:@"John C Smith"]);
  EXPECT_TRUE([[am email] isEqualToString:@"john@chromium.org"]);
  EXPECT_TRUE([[am companyName] isEqualToString:@"Google Inc."]);
  EXPECT_TRUE([[am addressLine1] isEqualToString:@"1122 Mountain View Road"]);
  EXPECT_TRUE([[am addressLine2] isEqualToString:@"Suite #1"]);
  EXPECT_TRUE([[am addressCity] isEqualToString:@"Mountain View"]);
  EXPECT_TRUE([[am addressState] isEqualToString:@"CA"]);
  EXPECT_TRUE([[am addressZip] isEqualToString:@"94111"]);
  EXPECT_TRUE([[am phoneWholeNumber] isEqualToString:@"014155552258"]);
  EXPECT_TRUE([[am faxWholeNumber] isEqualToString:@"024087172258"]);

  [sheet save:nil];
  [controller_ closeDialog];

  ASSERT_TRUE(observer_.hit_);
  ASSERT_TRUE(observer_.profiles_.size() == 1);

  ASSERT_EQ(0, observer_.profiles_[0].Compare(*profiles()[0]));
}

TEST_F(AutoFillDialogControllerTest, CreditCardDataMutation) {
  CreditCard credit_card;
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("DCH"));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
                      ASCIIToUTF16("1234 5678 9101 1121"));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("01"));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                      ASCIIToUTF16("2012"));
  credit_cards().push_back(&credit_card);

  LoadDialog();
  [controller_ selectCreditCardAtIndex:0];
  [controller_ editSelection:nil];

  AutoFillCreditCardSheetController* sheet =
      [controller_ creditCardSheetController];
  ASSERT_TRUE(sheet != nil);
  AutoFillCreditCardModel* cm = [sheet creditCardModel];
  EXPECT_TRUE([[cm nameOnCard] isEqualToString:@"DCH"]);
  EXPECT_TRUE([[cm creditCardNumber] isEqualToString:@"1234567891011121"]);
  EXPECT_TRUE([[cm expirationMonth] isEqualToString:@"01"]);
  EXPECT_TRUE([[cm expirationYear] isEqualToString:@"2012"]);

  // Check that user-visible text is obfuscated.
  NSTextField* numberField = [sheet creditCardNumberField];
  ASSERT_TRUE(numberField != nil);
  EXPECT_TRUE([[numberField stringValue] isEqualToString:@"************1121"]);

  [sheet save:nil];
  [controller_ closeDialog];

  ASSERT_TRUE(observer_.hit_);
  ASSERT_TRUE(observer_.credit_cards_.size() == 1);
  ASSERT_EQ(0, observer_.credit_cards_[0].Compare(*credit_cards()[0]));
}

TEST_F(AutoFillDialogControllerTest, TwoProfiles) {
  AutoFillProfile profile1;
  profile1.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile1);
  AutoFillProfile profile2;
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Bob"));
  profiles().push_back(&profile2);
  LoadDialog();
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Sizes should match.  And should be 2.
  ASSERT_EQ([controller_ profiles].size(), profiles().size());
  ASSERT_EQ([controller_ profiles].size(), 2UL);

  // Contents should match.  With the exception of the |unique_id|.
  for (size_t i = 0, count = profiles().size(); i < count; i++) {
    ASSERT_EQ(0, [controller_ profiles][i].Compare(*profiles()[i]));
  }
}

TEST_F(AutoFillDialogControllerTest, TwoCreditCards) {
  CreditCard credit_card1;
  credit_card1.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card1);
  CreditCard credit_card2;
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Bob"));
  credit_cards().push_back(&credit_card2);
  LoadDialog();
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Sizes should match.  And should be 2.
  ASSERT_EQ([controller_ creditCards].size(), credit_cards().size());
  ASSERT_EQ([controller_ creditCards].size(), 2UL);

  // Contents should match.  With the exception of the |unique_id|.
  for (size_t i = 0, count = credit_cards().size(); i < count; i++) {
    ASSERT_EQ(0, [controller_ creditCards][i].Compare(*credit_cards()[i]));
  }
}

TEST_F(AutoFillDialogControllerTest, AddNewProfile) {
  AutoFillProfile profile;
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile);
  LoadDialog();
  [controller_ addNewAddress:nil];
  AutoFillAddressSheetController* sheet = [controller_ addressSheetController];
  ASSERT_TRUE(sheet != nil);
  AutoFillAddressModel* model = [sheet addressModel];
  ASSERT_TRUE(model != nil);
  [model setFullName:@"Don"];
  [sheet save:nil];
  [controller_ closeDialog];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should be different.  New size should be 2.
  ASSERT_NE(observer_.profiles_.size(), profiles().size());
  ASSERT_EQ(observer_.profiles_.size(), 2UL);

  // New address should match.  Don't compare labels.
  AutoFillProfile new_profile;
  new_profile.SetInfo(AutoFillType(NAME_FULL), ASCIIToUTF16("Don"));
  ASSERT_EQ(0, observer_.profiles_[1].Compare(new_profile));
}

TEST_F(AutoFillDialogControllerTest, AddNewCreditCard) {
  CreditCard credit_card;
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card);
  LoadDialog();
  [controller_ addNewCreditCard:nil];
  AutoFillCreditCardSheetController* sheet =
      [controller_ creditCardSheetController];
  ASSERT_TRUE(sheet != nil);
  AutoFillCreditCardModel* model = [sheet creditCardModel];
  ASSERT_TRUE(model != nil);
  [model setNameOnCard:@"Don"];
  [sheet save:nil];
  [controller_ closeDialog];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should be different.  New size should be 2.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards().size());
  ASSERT_EQ(observer_.credit_cards_.size(), 2UL);

  // New credit card should match.  Don't compare labels.
  CreditCard new_credit_card;
  new_credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Don"));
  ASSERT_EQ(0, observer_.credit_cards_[1].Compare(new_credit_card));
}

TEST_F(AutoFillDialogControllerTest, AddNewEmptyProfile) {
  AutoFillProfile profile;
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile);
  LoadDialog();
  [controller_ addNewAddress:nil];
  AutoFillAddressSheetController* sheet = [controller_ addressSheetController];
  ASSERT_TRUE(sheet != nil);
  [sheet save:nil];
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Empty profile should not be saved.
  ASSERT_EQ(0UL, observer_.profiles_.size());
}

TEST_F(AutoFillDialogControllerTest, AddNewEmptyCreditCard) {
  CreditCard credit_card;
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card);
  LoadDialog();
  [controller_ addNewCreditCard:nil];
  AutoFillCreditCardSheetController* sheet =
      [controller_ creditCardSheetController];
  ASSERT_TRUE(sheet != nil);
  [sheet save:nil];
  [controller_ closeDialog];

  // Should hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Empty credit card should not be saved.
  ASSERT_EQ(0UL, observer_.credit_cards_.size());
}

TEST_F(AutoFillDialogControllerTest, DeleteProfile) {
  AutoFillProfile profile;
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile);
  LoadDialog();
  [controller_ selectAddressAtIndex:0];
  [controller_ deleteSelection:nil];
  [controller_ closeDialog];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should be different.  New size should be 0.
  ASSERT_NE(observer_.profiles_.size(), profiles().size());
  ASSERT_EQ(observer_.profiles_.size(), 0UL);
}

TEST_F(AutoFillDialogControllerTest, DeleteCreditCard) {
  CreditCard credit_card;
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card);
  LoadDialog();
  [controller_ selectCreditCardAtIndex:0];
  [controller_ deleteSelection:nil];
  [controller_ closeDialog];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should be different.  New size should be 0.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards().size());
  ASSERT_EQ(observer_.credit_cards_.size(), 0UL);
}

TEST_F(AutoFillDialogControllerTest, TwoProfilesDeleteOne) {
  AutoFillProfile profile;
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile);
  AutoFillProfile profile2;
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Bob"));
  profiles().push_back(&profile2);
  LoadDialog();
  [controller_ selectAddressAtIndex:1];
  [controller_ deleteSelection:nil];
  [controller_ closeDialog];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should be different.  New size should be 1.
  ASSERT_NE(observer_.profiles_.size(), profiles().size());
  ASSERT_EQ(observer_.profiles_.size(), 1UL);
  ASSERT_EQ(0, observer_.profiles_[0].Compare(profile));
}

TEST_F(AutoFillDialogControllerTest, TwoCreditCardsDeleteOne) {
  CreditCard credit_card;
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card);
  CreditCard credit_card2;
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Bob"));
  credit_cards().push_back(&credit_card2);
  LoadDialog();
  [controller_ selectCreditCardAtIndex:1];
  [controller_ deleteSelection:nil];
  [controller_ closeDialog];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should be different.  New size should be 1.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards().size());
  ASSERT_EQ(observer_.credit_cards_.size(), 1UL);

  // First credit card should match.
  ASSERT_EQ(0, observer_.credit_cards_[0].Compare(credit_card));
}

TEST_F(AutoFillDialogControllerTest, DeleteMultiple) {
  AutoFillProfile profile;
  profile.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Joe"));
  profiles().push_back(&profile);
  AutoFillProfile profile2;
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Bob"));
  profiles().push_back(&profile2);

  CreditCard credit_card;
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  credit_cards().push_back(&credit_card);
  CreditCard credit_card2;
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Bob"));
  credit_cards().push_back(&credit_card2);

  LoadDialog();
  [controller_ selectAddressAtIndex:1];
  [controller_ addSelectedCreditCardAtIndex:0];
  ASSERT_FALSE([controller_ editButtonEnabled]);
  [controller_ deleteSelection:nil];
  [controller_ selectAddressAtIndex:0];
  ASSERT_TRUE([controller_ editButtonEnabled]);
  [controller_ closeDialog];

  // Should hit our observer.
  ASSERT_TRUE(observer_.hit_);

  // Sizes should be different.  New size should be 1.
  ASSERT_NE(observer_.profiles_.size(), profiles().size());
  ASSERT_EQ(observer_.profiles_.size(), 1UL);

  // Sizes should be different.  New size should be 1.
  ASSERT_NE(observer_.credit_cards_.size(), credit_cards().size());
  ASSERT_EQ(observer_.credit_cards_.size(), 1UL);

  // Profiles should match.
  ASSERT_EQ(0, observer_.profiles_[0].Compare(profile));

  // Second credit card should match.
  ASSERT_EQ(0, observer_.credit_cards_[0].Compare(credit_card2));
}

// Auxilliary profiles are enabled by default.
TEST_F(AutoFillDialogControllerTest, AuxiliaryProfilesTrue) {
  LoadDialog();
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_TRUE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

TEST_F(AutoFillDialogControllerTest, AuxiliaryProfilesFalse) {
  helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  LoadDialog();
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_FALSE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

TEST_F(AutoFillDialogControllerTest, AuxiliaryProfilesChanged) {
  helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  LoadDialog();
  [controller_ setAuxiliaryEnabled:YES];
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_TRUE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

TEST_F(AutoFillDialogControllerTest, WaitForDataToLoad) {
  AutoFillProfile profile;
  profiles().push_back(&profile);
  CreditCard credit_card;
  credit_cards().push_back(&credit_card);
  helper_.test_profile_->test_manager_->test_data_is_loaded_ = false;
  LoadDialog();
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Sizes should match.
  ASSERT_EQ([controller_ profiles].size(), profiles().size());
  ASSERT_EQ([controller_ creditCards].size(), credit_cards().size());

  // Contents should match.
  size_t i = 0;
  size_t count = profiles().size();
  for (i = 0; i < count; i++) {
    // Do not compare labels.  Label is a derived field.
    [controller_ profiles][i].set_label(string16());
    profiles()[i]->set_label(string16());
    ASSERT_EQ([controller_ profiles][i], *profiles()[i]);
  }
  count = credit_cards().size();
  for (i = 0; i < count; i++) {
    ASSERT_EQ([controller_ creditCards][i], *credit_cards()[i]);
  }
}

// AutoFill is enabled by default.
TEST_F(AutoFillDialogControllerTest, AutoFillEnabledTrue) {
  LoadDialog();
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // AutoFill enabled setting should be unchanged.
  ASSERT_TRUE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillEnabled));
}

TEST_F(AutoFillDialogControllerTest, AutoFillEnabledFalse) {
  helper_.profile()->GetPrefs()->SetBoolean(prefs::kAutoFillEnabled, false);
  LoadDialog();
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // AutoFill enabled setting should be unchanged.
  ASSERT_FALSE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillEnabled));
}

TEST_F(AutoFillDialogControllerTest, AutoFillEnabledChanged) {
  helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  LoadDialog();
  [controller_ setAutoFillEnabled:YES];
  [controller_ closeDialog];

  // Should not hit our observer.
  ASSERT_FALSE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_TRUE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillEnabled));
}

TEST_F(AutoFillDialogControllerTest, PolicyRefresh) {
  LoadDialog();
  ASSERT_FALSE([controller_ autoFillManaged]);

  // Disable AutoFill through configuration policy.
  helper_.profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kAutoFillEnabled, Value::CreateBooleanValue(false));
  ASSERT_TRUE([controller_ autoFillManaged]);
  ASSERT_FALSE([controller_ autoFillEnabled]);

  // Enabling through policy should switch to enabled but not editable.
  helper_.profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kAutoFillEnabled, Value::CreateBooleanValue(true));
  ASSERT_TRUE([controller_ autoFillManaged]);
  ASSERT_TRUE([controller_ autoFillEnabled]);

  [controller_ closeDialog];
}

TEST_F(AutoFillDialogControllerTest, PolicyDisabledAndSave) {
  // Disable AutoFill through configuration policy.
  helper_.profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kAutoFillEnabled, Value::CreateBooleanValue(false));
  helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  LoadDialog();

  // Save should be disabled.
  ASSERT_TRUE([controller_ autoFillManagedAndDisabled]);

  [controller_ setAuxiliaryEnabled:YES];
  [controller_ closeDialog];

  // Observer should not have been called.
  ASSERT_FALSE(observer_.hit_);

  // Auxiliary profiles setting should be unchanged.
  ASSERT_FALSE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

TEST_F(AutoFillDialogControllerTest, PolicyEnabledAndSave) {
  // Enable AutoFill through configuration policy.
  helper_.profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kAutoFillEnabled, Value::CreateBooleanValue(true));
  helper_.profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  LoadDialog();

  // Save should be enabled.
  ASSERT_FALSE([controller_ autoFillManagedAndDisabled]);

  [controller_ setAuxiliaryEnabled:YES];
  [controller_ closeDialog];

  // Observer should not have been notified.
  ASSERT_FALSE(observer_.hit_);

  // Auxiliary profiles setting should have been saved.
  ASSERT_TRUE(helper_.profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
}

}  // namespace
