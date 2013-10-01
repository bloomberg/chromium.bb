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
#if defined(OS_MACOSX)
  // Default on Mac (for now) is to use sandbox servers.
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
#else
  EXPECT_EQ("https://wallet.google.com/online/v2/wallet/"
            "autocheckout/v1/getWalletItemsJwtless",
            GetGetWalletItemsUrl().spec());
  EXPECT_EQ("https://wallet.google.com/online-secure/v2/"
            "autocheckout/v1/getFullWalletJwtless?s7e=otp",
            GetGetFullWalletUrl().spec());
  EXPECT_EQ("https://wallet.google.com/manage/paymentMethods",
            GetManageInstrumentsUrl().spec());
  EXPECT_EQ("https://wallet.google.com/manage/settings/addresses",
            GetManageAddressesUrl().spec());
  EXPECT_EQ("https://wallet.google.com/online/v2/wallet/"
            "autocheckout/v1/acceptLegalDocument",
            GetAcceptLegalDocumentsUrl().spec());
  EXPECT_EQ("https://wallet.google.com/online-secure/v2/"
            "autocheckout/v1/authenticateInstrument?s7e=cvn",
            GetAuthenticateInstrumentUrl().spec());
  EXPECT_EQ("https://wallet.google.com/online/v2/wallet/"
            "autocheckout/v1/reportStatus",
            GetSendStatusUrl().spec());
  EXPECT_EQ("https://wallet.google.com/online/v2/wallet/"
            "autocheckout/v1/saveToWallet",
            GetSaveToWalletNoEscrowUrl().spec());
  EXPECT_EQ("https://wallet.google.com/online-secure/v2/"
            "autocheckout/v1/saveToWallet?s7e=card_number%3Bcvn",
            GetSaveToWalletUrl().spec());
  EXPECT_EQ("https://wallet.google.com/online/v2/"
            "passiveauth?isChromePayments=true",
            GetPassiveAuthUrl().spec());
#endif
}

TEST(WalletServiceUrl, IsUsingProd) {
#if defined(OS_MACOSX)
  EXPECT_FALSE(IsUsingProd());
#else
  EXPECT_TRUE(IsUsingProd());
#endif

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kWalletServiceUseSandbox, "1");
  EXPECT_FALSE(IsUsingProd());

  const GURL sandbox_get_items_url = GetGetWalletItemsUrl();
  const GURL fake_service_url = GURL("http://goo.gl");
  command_line->AppendSwitchASCII(switches::kWalletServiceUrl,
                                  fake_service_url.spec());

  const GURL flag_get_items_url = GetGetWalletItemsUrl();
  ASSERT_NE(sandbox_get_items_url, flag_get_items_url);
  EXPECT_EQ(fake_service_url.GetOrigin(), flag_get_items_url.GetOrigin());
}

}  // namespace wallet
}  // namespace autofill
