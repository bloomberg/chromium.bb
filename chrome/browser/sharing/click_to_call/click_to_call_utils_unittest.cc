// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/mock_sharing_service.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_handler_registry.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

using SharingMessage = chrome_browser_sharing::SharingMessage;

namespace {

const char kEmptyTelUrl[] = "tel:";
const char kTelUrl[] = "tel:+9876543210";
const char kNonTelUrl[] = "https://google.com";

const char kSelectionTextWithNumber[] = "9876543210";

class ClickToCallUtilsTest : public testing::Test {
 public:
  ClickToCallUtilsTest() = default;

  ~ClickToCallUtilsTest() override = default;

  void SetUp() override {
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&ClickToCallUtilsTest::CreateService,
                                       base::Unretained(this)));
  }

  void ExpectClickToCallDisabledForSelectionText(
      const std::string& selection_text,
      bool use_incognito_profile = false) {
    Profile* profile_to_use =
        use_incognito_profile ? profile_.GetOffTheRecordProfile() : &profile_;
    base::Optional<std::string> phone_number =
        ExtractPhoneNumberForClickToCall(profile_to_use, selection_text);
    EXPECT_FALSE(phone_number.has_value())
        << " Found phone number: " << phone_number.value()
        << " in selection text: " << selection_text;
  }

 protected:
  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    return create_service_ ? std::make_unique<MockSharingService>() : nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  bool create_service_ = true;

  DISALLOW_COPY_AND_ASSIGN(ClickToCallUtilsTest);
};

}  // namespace

TEST_F(ClickToCallUtilsTest, NoSharingService_DoNotOfferAnyMenu) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  create_service_ = false;
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL(kTelUrl)));
  ExpectClickToCallDisabledForSelectionText(kSelectionTextWithNumber);
}

TEST_F(ClickToCallUtilsTest, UIFlagDisabled_DoNotOfferAnyMenu) {
  scoped_feature_list_.InitAndDisableFeature(kClickToCallUI);
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL(kTelUrl)));
  ExpectClickToCallDisabledForSelectionText(kSelectionTextWithNumber);
}

TEST_F(ClickToCallUtilsTest, PolicyDisabled_DoNotOfferAnyMenu) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  profile_.GetPrefs()->SetBoolean(prefs::kClickToCallEnabled, false);
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL(kTelUrl)));
  ExpectClickToCallDisabledForSelectionText(kSelectionTextWithNumber);
}

TEST_F(ClickToCallUtilsTest, IncognitoProfile_DoNotOfferAnyMenu) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  EXPECT_FALSE(ShouldOfferClickToCallForURL(profile_.GetOffTheRecordProfile(),
                                            GURL(kTelUrl)));
  ExpectClickToCallDisabledForSelectionText(kSelectionTextWithNumber,
                                            /*use_incognito_profile =*/true);
}

TEST_F(ClickToCallUtilsTest, EmptyTelLink_DoNotOfferForLink) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL(kEmptyTelUrl)));
}

TEST_F(ClickToCallUtilsTest, TelLink_OfferForLink) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  EXPECT_TRUE(ShouldOfferClickToCallForURL(&profile_, GURL(kTelUrl)));
}

TEST_F(ClickToCallUtilsTest, NonTelLink_DoNotOfferForLink) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  EXPECT_FALSE(ShouldOfferClickToCallForURL(&profile_, GURL(kNonTelUrl)));
}

TEST_F(ClickToCallUtilsTest,
       SelectionText_ValidPhoneNumberRegex_OfferForSelection) {
  scoped_feature_list_.InitWithFeatures({kClickToCallUI},
                                        {kClickToCallDetectionV2});

  // Stores a mapping of selected text to expected phone number parsed.
  std::map<std::string, std::string> expectations;
  // Selection text only consists of the phone number.
  expectations.emplace("9876543210", "9876543210");
  // Check for phone number at end of text.
  expectations.emplace("Call on 9876543210", "9876543210");
  // Check for international number with a space between code and phone number.
  expectations.emplace("Call +44 9876543210 now", "+44 9876543210");
  // Check for international number without spacing.
  expectations.emplace("call (+44)9876543210 now", "(+44)9876543210");
  // Check for dashes.
  expectations.emplace("(+44)987-654-3210 now", "(+44)987-654-3210");
  // Check for spaces and dashes.
  expectations.emplace("call (+44) 987 654-3210 now", "(+44) 987 654-3210");
  // The first number is always returned.
  expectations.emplace("9876543210 and 9999888877", "9876543210");
  // Spaces are allowed in between numbers.
  expectations.emplace("9 8 7 6 5 4 3 2 1 0", "9 8 7 6 5 4 3 2 1 0");
  // Two spaces in between.
  expectations.emplace("9  8 7 6 5  4 3 2 1 0", "9  8 7 6 5  4 3 2 1 0");
  // Non breaking spaces around number.
  expectations.emplace("\u00A09876543210\u00A0", "9876543210");
  // Example for a credit card
  expectations.emplace("4111 1111 1111 1111", "4111 1111 1111 1111");
  // Chrome version string
  expectations.emplace("78.0.3904.108", "78.0.3904.108");

  for (auto& expectation : expectations) {
    base::Optional<std::string> phone_number =
        ExtractPhoneNumberForClickToCall(&profile_, expectation.first);
    ASSERT_NE(base::nullopt, phone_number);
    EXPECT_EQ(expectation.second, phone_number.value());
  }
}

TEST_F(ClickToCallUtilsTest,
       SelectionText_InvalidPhoneNumberRegex_DoNotOfferForSelection) {
  scoped_feature_list_.InitWithFeatures({kClickToCallUI},
                                        {kClickToCallDetectionV2});
  std::vector<std::string> invalid_selection_texts;

  // Does not contain any number.
  invalid_selection_texts.push_back("Call me maybe");
  // We only parse smaller text sizes to avoid performance impact on Chromium.
  invalid_selection_texts.push_back(
      "This is a huge text. It also contains a phone number 9876543210");
  // Although this is a valid number, its not caught by the regex.
  invalid_selection_texts.push_back("+44 1800-FLOWERS");
  // Number does not start as new word.
  invalid_selection_texts.push_back("No space9876543210");
  // Minimum length for regex match not satisfied.
  invalid_selection_texts.push_back("Small number 98765");
  // Number does not start as new word.
  invalid_selection_texts.push_back("Buy for $9876543210");
  // More than two spaces in between.
  invalid_selection_texts.push_back("9   8   7   6   5   4    3   2   1     0");
  // Space dash space formatting.
  invalid_selection_texts.push_back("999 - 999 - 9999");

  for (auto& text : invalid_selection_texts)
    ExpectClickToCallDisabledForSelectionText(text);
}

TEST_F(ClickToCallUtilsTest, SelectionText_LowConfidenceModifiedRegex_Matches) {
  scoped_feature_list_.InitWithFeatures(
      {kClickToCallUI, kClickToCallDetectionV2}, {});

  // Stores a mapping of selected text to expected phone number parsed.
  std::map<std::string, std::string> expectations = {
      {"+91 77997 12345", "+91 77997 12345"},
      {"(+0091) 040 12345678", "(+0091) 040 12345678"},
      {"+1 800 444 4444", "+1 800 444 4444"},
      {"754-1234", "754-1234"},
      {"+55-955-1234-1234 (landline)", "+55-955-1234-1234"},
      {"+44(0)20-1234 1234", "+44(0)20-1234 1234"},
      {"07700123123", "07700123123"},
      {"Call +49 231 1234567 now!", "+49 231 1234567"},
      {"tel:+49-89-636-12345", "+49-89-636-12345"},
      {"Number (021) 12345678", "(021) 12345678"},
      {"(private) +90 312 123 12 12", "+90 312 123 12 12"},
      {"+34 913 12 12 12", "+34 913 12 12 12"},
      {"(000)\u00A00000000", "(000)\u00A00000000"},
  };

  for (auto& expectation : expectations) {
    base::Optional<std::string> phone_number =
        ExtractPhoneNumberForClickToCall(&profile_, expectation.first);
    EXPECT_EQ(expectation.second, phone_number.value_or(""));
  }
}

TEST_F(ClickToCallUtilsTest, SelectionText_LowConfidenceModifiedRegex_NoMatch) {
  scoped_feature_list_.InitWithFeatures(
      {kClickToCallUI, kClickToCallDetectionV2}, {});
  std::vector<std::string> invalid_selection_texts = {
      // Example for a credit card
      "4111 1111 1111 1111",
      // Chrome version string
      "78.0.3904.108",
      // Too many spaces
      "9 8 7 6 5 4 3 2 1 0",
  };

  for (auto& text : invalid_selection_texts)
    ExpectClickToCallDisabledForSelectionText(text);
}
