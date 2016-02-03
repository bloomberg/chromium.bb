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

namespace {

bool IsUsingProd() {
  return GetManageAddressesUrl(1).GetWithEmptyPath() ==
         GURL("https://wallet.google.com/");
}
}

TEST(WalletServiceSandboxUrl, CheckSandboxUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "1");

  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/w/1/paymentMethods",
            GetManageInstrumentsUrl(1).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/w/1/settings/"
            "addresses",
            GetManageAddressesUrl(1).spec());
}

TEST(WalletServiceSandboxUrl, CheckProdUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "0");

  EXPECT_EQ("https://wallet.google.com/manage/w/1/paymentMethods",
            GetManageInstrumentsUrl(1).spec());
  EXPECT_EQ("https://wallet.google.com/manage/w/1/settings/addresses",
            GetManageAddressesUrl(1).spec());
}

// Disabling, see http://crbug.com/581880.
TEST(WalletServiceUrl, DISABLED_DefaultsToProd) {
#if defined(ENABLE_PROD_WALLET_SERVICE)
  EXPECT_TRUE(IsUsingProd());
#else
  EXPECT_FALSE(IsUsingProd());
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kReduceSecurityForTesting);
  EXPECT_FALSE(IsUsingProd());

  command_line->AppendSwitchASCII(switches::kWalletServiceUseSandbox, "0");
  EXPECT_TRUE(IsUsingProd());
}

TEST(WalletServiceUrl, IsUsingProd) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kWalletServiceUseSandbox, "1");
  EXPECT_FALSE(IsUsingProd());
}

}  // namespace wallet
}  // namespace autofill
