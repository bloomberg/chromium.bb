// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace wallet {

TEST(WalletServiceSandboxUrl, CheckSandboxUrls) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "1");

  EXPECT_EQ("https://wallet-web.sandbox.google.com/online/v2/u/1/wallet/"
            "autocheckout/v1/getWalletItemsJwtless",
           GetGetWalletItemsUrl(1).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online-secure/v2/u/1/"
            "autocheckout/v1/getFullWalletJwtless?s7e=otp",
            GetGetFullWalletUrl(1).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/w/1/paymentMethods",
            GetManageInstrumentsUrl(1).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/w/1/settings/"
            "addresses",
            GetManageAddressesUrl(1).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online/v2/u/1/wallet/"
            "autocheckout/v1/acceptLegalDocument",
            GetAcceptLegalDocumentsUrl(1).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online-secure/v2/u/2/"
            "autocheckout/v1/authenticateInstrument?s7e=cvn",
            GetAuthenticateInstrumentUrl(2).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online/v2/u/1/wallet/"
            "autocheckout/v1/saveToWallet",
            GetSaveToWalletNoEscrowUrl(1).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online-secure/v2/u/1/"
            "autocheckout/v1/saveToWallet?s7e=card_number%3Bcvn",
            GetSaveToWalletUrl(1).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online/v2/u/1/"
            "passiveauth?isChromePayments=true",
            GetPassiveAuthUrl(1).spec());
}

TEST(WalletServiceSandboxUrl, CheckProdUrls) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "0");

  EXPECT_EQ("https://wallet.google.com/online/v2/u/1/wallet/"
            "autocheckout/v1/getWalletItemsJwtless",
            GetGetWalletItemsUrl(1).spec());
  EXPECT_EQ("https://wallet.google.com/online-secure/v2/u/1/"
            "autocheckout/v1/getFullWalletJwtless?s7e=otp",
            GetGetFullWalletUrl(1).spec());
  EXPECT_EQ("https://wallet.google.com/manage/w/1/paymentMethods",
            GetManageInstrumentsUrl(1).spec());
  EXPECT_EQ("https://wallet.google.com/manage/w/1/settings/addresses",
            GetManageAddressesUrl(1).spec());
  EXPECT_EQ("https://wallet.google.com/online/v2/u/1/wallet/"
            "autocheckout/v1/acceptLegalDocument",
            GetAcceptLegalDocumentsUrl(1).spec());
  EXPECT_EQ("https://wallet.google.com/online-secure/v2/u/4/"
            "autocheckout/v1/authenticateInstrument?s7e=cvn",
            GetAuthenticateInstrumentUrl(4).spec());
  EXPECT_EQ("https://wallet.google.com/online/v2/u/1/wallet/"
            "autocheckout/v1/saveToWallet",
            GetSaveToWalletNoEscrowUrl(1).spec());
  EXPECT_EQ("https://wallet.google.com/online-secure/v2/u/1/"
            "autocheckout/v1/saveToWallet?s7e=card_number%3Bcvn",
            GetSaveToWalletUrl(1).spec());
  EXPECT_EQ("https://wallet.google.com/online/v2/u/1/"
            "passiveauth?isChromePayments=true",
            GetPassiveAuthUrl(1).spec());
}

TEST(WalletServiceUrl, DefaultsToProd) {
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(IsUsingProd());
#else
  EXPECT_FALSE(IsUsingProd());
#endif

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kReduceSecurityForTesting);
  EXPECT_FALSE(IsUsingProd());

  command_line->AppendSwitchASCII(switches::kWalletServiceUseSandbox, "0");
  EXPECT_TRUE(IsUsingProd());
}

TEST(WalletServiceUrl, IsUsingProd) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kWalletServiceUseSandbox, "1");
  EXPECT_FALSE(IsUsingProd());

  const GURL sandbox_get_items_url = GetGetWalletItemsUrl(0);
  const GURL fake_service_url = GURL("http://goo.gl");
  command_line->AppendSwitchASCII(switches::kWalletServiceUrl,
                                  fake_service_url.spec());

  const GURL flag_get_items_url = GetGetWalletItemsUrl(0);
  EXPECT_NE(sandbox_get_items_url, flag_get_items_url);
  EXPECT_EQ(fake_service_url.GetOrigin(), flag_get_items_url.GetOrigin());
}

TEST(WalletServiceUrl, IsSignInContinueUrl) {
  EXPECT_TRUE(GetSignInContinueUrl().SchemeIsSecure());

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kWalletServiceUseSandbox, "1");

  // authuser query param is respected.
  const char sign_in_url[] = "https://wallet-web.sandbox.google.com/online/v2/"
      "u/0/passiveauth?isChromePayments=true&authuser=4";
  size_t user_index = 100;
  EXPECT_TRUE(IsSignInContinueUrl(GURL(sign_in_url), &user_index));
  EXPECT_EQ(4U, user_index);

  // No authuser query param means 0 is assumed.
  user_index = 101;
  const char sign_in_url_no_user[] = "https://wallet-web.sandbox.google.com/"
      "online/v2/u/0/passiveauth?isChromePayments=true";
  EXPECT_TRUE(IsSignInContinueUrl(GURL(sign_in_url_no_user), &user_index));
  EXPECT_EQ(0U, user_index);

  // A authuser query param that doesn't parse means 0 is assumed.
  user_index = 102;
  const char sign_in_url_bad_user[] = "https://wallet-web.sandbox.google.com/"
      "online/v2/u/0/passiveauth?isChromePayments=true&authuser=yolo";
  EXPECT_TRUE(IsSignInContinueUrl(GURL(sign_in_url_bad_user), &user_index));
  EXPECT_EQ(0U, user_index);

  const char not_a_sign_in_url[] = "https://wallet-web.sandbox.google.com/"
      "online/v2/u/0/example";
  EXPECT_FALSE(IsSignInContinueUrl(GURL(not_a_sign_in_url), &user_index));
}

TEST(WalletServiceUrl, IsSignInRelatedUrl) {
  EXPECT_TRUE(IsSignInRelatedUrl(GetSignInUrl()));
  EXPECT_TRUE(IsSignInRelatedUrl(GURL("https://accounts.youtube.com")));
  EXPECT_TRUE(IsSignInRelatedUrl(GURL("https://accounts.youtube.com/")));
  EXPECT_TRUE(IsSignInRelatedUrl(GURL("https://accounts.google.com")));
  EXPECT_FALSE(IsSignInRelatedUrl(GURL("http://google.com")));
}

}  // namespace wallet
}  // namespace autofill
