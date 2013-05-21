// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/wallet/wallet_service_url.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace wallet {

TEST(WalletServiceUrl, CheckDefaultUrls) {
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
           "autocheckout/v1/getWalletItemsJwtless",
            GetGetWalletItemsUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
            "autocheckout/v1/getFullWalletJwtless",
            GetGetFullWalletUrl().spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/w/0/#paymentMethods:",
            GetManageInstrumentsUrl().spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/w/0/"
            "#settings:addresses",
            GetManageAddressesUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
            "autocheckout/v1/acceptLegalDocument",
            GetAcceptLegalDocumentsUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
            "autocheckout/v1/authenticateInstrument",
            GetAuthenticateInstrumentUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
            "autocheckout/v1/reportStatus",
            GetSendStatusUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/wallet/"
            "autocheckout/v1/saveToWallet",
            GetSaveToWalletUrl().spec());
  EXPECT_EQ("https://payments-form-dogfood.sandbox.google.com/online/v2/"
            "passiveauth",
            GetPassiveAuthUrl().spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/online-secure/"
            "temporarydata/cvv?s7e=cvv",
            GetEncryptionUrl().spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/checkout/dehEfe?"
            "s7e=cardNumber%3Bcvv",
            GetEscrowUrl().spec());
}

}  // namespace wallet
}  // namespace autofill
