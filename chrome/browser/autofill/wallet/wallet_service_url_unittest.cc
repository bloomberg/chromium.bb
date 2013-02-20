// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/wallet_service_url.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace wallet {

TEST(WalletServiceUrl, CheckDefaultUrls) {
  ASSERT_EQ("https://wallet.google.com/online/v2/wallet/autocheckout/v1/"
            "getWalletItemsJwtless",
            GetGetWalletItemsUrl().spec());
  ASSERT_EQ("https://wallet.google.com/online/v2/wallet/autocheckout/v1/"
            "getFullWalletJwtless",
            GetGetFullWalletUrl().spec());
  ASSERT_EQ("https://wallet.google.com/online/v2/wallet/autocheckout/v1/"
            "acceptLegalDocuments",
            GetAcceptLegalDocumentsUrl().spec());
  ASSERT_EQ("https://wallet.google.com/online/v2/wallet/autocheckout/v1/"
            "reportStatus",
            GetSendStatusUrl().spec());
  ASSERT_EQ("https://wallet.google.com/online/v2/wallet/autocheckout/v1/"
            "saveToWallet",
            GetSaveToWalletUrl().spec());
  ASSERT_EQ("https://wallet.google.com/online/v2/passiveauth",
            GetPassiveAuthUrl().spec());
  ASSERT_EQ("https://wallet.google.com/online-secure/temporarydata/cvv?s7e=cvv",
            GetEncryptionUrl().spec());
  ASSERT_EQ("https://wallet.google.com/dehEfe?s7e=cardNumber%3Bcvv",
            GetEscrowUrl().spec());
}

}  // namespace wallet
}  // namespace autofill
