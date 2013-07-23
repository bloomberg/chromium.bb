// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace wallet {

TEST(WalletServiceUrl, CheckDefaultUrls) {
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
           "autocheckout/v1/getWalletItemsJwtless",
            GetGetWalletItemsUrl().spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online-secure/v2/"
            "autocheckout/v1/getFullWalletJwtless?s7e=otp",
            GetGetFullWalletUrl().spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/paymentMethods",
            GetManageInstrumentsUrl().spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/settings/addresses",
            GetManageAddressesUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
            "autocheckout/v1/acceptLegalDocument",
            GetAcceptLegalDocumentsUrl().spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online-secure/v2/"
            "autocheckout/v1/authenticateInstrument?s7e=cvn",
            GetAuthenticateInstrumentUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
            "autocheckout/v1/reportStatus",
            GetSendStatusUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
            "autocheckout/v1/saveToWallet",
            GetSaveToWalletNoEscrowUrl().spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online-secure/v2/"
            "autocheckout/v1/saveToWallet?s7e=card_number%3Bcvn",
            GetSaveToWalletUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/"
            "passiveauth?isChromePayments=true",
            GetPassiveAuthUrl().spec());
}

TEST(WalletServiceUrl, IsUsingProd) {
  // The sandbox servers are the default (for now). Update if this changes.
  EXPECT_FALSE(IsUsingProd());

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kWalletServiceUseProd);
  EXPECT_TRUE(IsUsingProd());

  const GURL prod_get_items_url = GetGetWalletItemsUrl();
  command_line->AppendSwitchASCII(switches::kWalletServiceUrl, "http://goo.gl");
  EXPECT_FALSE(IsUsingProd());

  ASSERT_NE(prod_get_items_url, GetGetWalletItemsUrl());
}

}  // namespace wallet
}  // namespace autofill
