// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"

#import <UIKit/UIKit.h>
#include <string>

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/credit_card.h"
#import "ios/web_view/internal/autofill/autofill_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVCreditCardTest : public PlatformTest {
 protected:
  CWVCreditCardTest() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        base::SysNSStringToUTF8(testing::kLocale), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ui::ResourceBundle& resource_bundle =
        ui::ResourceBundle::GetSharedInstance();

    // Don't load 100P resource since no @1x devices are supported.
    if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
      base::FilePath pak_file_200;
      base::PathService::Get(base::DIR_MODULE, &pak_file_200);
      pak_file_200 =
          pak_file_200.Append(FILE_PATH_LITERAL("web_view_200_percent.pak"));
      resource_bundle.AddDataPackFromPath(pak_file_200, ui::SCALE_FACTOR_200P);
    }
    if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_300P)) {
      base::FilePath pak_file_300;
      base::PathService::Get(base::DIR_MODULE, &pak_file_300);
      pak_file_300 =
          pak_file_300.Append(FILE_PATH_LITERAL("web_view_300_percent.pak"));
      resource_bundle.AddDataPackFromPath(pak_file_300, ui::SCALE_FACTOR_300P);
    }
  }

  ~CWVCreditCardTest() override { ui::ResourceBundle::CleanupSharedInstance(); }
};

// Tests CWVCreditCard initialization.
TEST_F(CWVCreditCardTest, Initialization) {
  CWVCreditCard* cwv_credit_card = [[CWVCreditCard alloc]
      initWithCreditCard:testing::CreateTestCreditCard()];

  EXPECT_NSEQ(testing::kCardHolderFullName, cwv_credit_card.cardHolderFullName);
  EXPECT_NSEQ(testing::kCardNumber, cwv_credit_card.cardNumber);
  EXPECT_NSEQ(testing::kNetworkName, cwv_credit_card.networkName);
  EXPECT_NSEQ(testing::kExpirationMonth, cwv_credit_card.expirationMonth);
  EXPECT_NSEQ(testing::kExpirationYear, cwv_credit_card.expirationYear);
  EXPECT_TRUE(cwv_credit_card.savedLocally);

  // It is not sufficient to simply test for networkIcon != nil because
  // ui::ResourceBundle will return a placeholder image at @1x scale if the
  // underlying resource id is not found. Since no @1x devices are supported
  // anymore, check to make sure the UIImage scale matches that of the UIScreen.
  EXPECT_TRUE(cwv_credit_card.networkIcon.scale == UIScreen.mainScreen.scale);
}

// Tests CWVCreditCard updates properties.
TEST_F(CWVCreditCardTest, ModifyProperties) {
  autofill::CreditCard credit_card(base::SysNSStringToUTF8(testing::kGuid),
                                   base::SysNSStringToUTF8(testing::kOrigin));
  CWVCreditCard* cwv_credit_card =
      [[CWVCreditCard alloc] initWithCreditCard:credit_card];
  cwv_credit_card.cardHolderFullName = testing::kCardHolderFullName;
  cwv_credit_card.cardNumber = testing::kCardNumber;
  cwv_credit_card.expirationMonth = testing::kExpirationMonth;
  cwv_credit_card.expirationYear = testing::kExpirationYear;

  EXPECT_NSEQ(testing::kCardHolderFullName, cwv_credit_card.cardHolderFullName);
  EXPECT_NSEQ(testing::kCardNumber, cwv_credit_card.cardNumber);
  EXPECT_NSEQ(testing::kNetworkName, cwv_credit_card.networkName);
  EXPECT_NSEQ(testing::kExpirationMonth, cwv_credit_card.expirationMonth);
  EXPECT_NSEQ(testing::kExpirationYear, cwv_credit_card.expirationYear);
  EXPECT_TRUE(cwv_credit_card.savedLocally);

  // It is not sufficient to simply test for networkIcon != nil because
  // ui::ResourceBundle will return a placeholder image at @1x scale if the
  // underlying resource id is not found. Since no @1x devices are supported
  // anymore, check to make sure the UIImage scale matches that of the UIScreen.
  EXPECT_TRUE(cwv_credit_card.networkIcon.scale == UIScreen.mainScreen.scale);
}

}  // namespace ios_web_view
