// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/wallet_service_url.h"

#include <string>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"

namespace {

const char kDefaultWalletServiceUrl[] = "https://wallet.google.com/online/v2/";

const char kDefaultWalletSecureServiceUrl[] =
    "https://wallet.google.com/online-secure/temporarydata/cvv?s7e=cvv";

GURL GetBaseWalletUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string base_wallet_service_url =
      command_line.GetSwitchValueASCII(switches::kWalletServiceUrl);
  return !base_wallet_service_url.empty() ? GURL(base_wallet_service_url) :
                                            GURL(kDefaultWalletServiceUrl);
}

GURL GetBaseAutocheckoutUrl() {
  return GetBaseWalletUrl().Resolve("wallet/autocheckout/");
}

}  // anonymous namespace

namespace wallet {

// TODO(ahutter): This shouldn't live in this class. See
// http://crbug.com/164281.
const char kApiKey[] = "abcdefg";

GURL GetGetWalletItemsUrl() {
  return GetBaseWalletUrl().Resolve(
      "wallet/autocheckout/getWalletItemsJwtless");
}

GURL GetGetFullWalletUrl() {
  return GetBaseAutocheckoutUrl().Resolve("getFullWalletJwtless");
}

GURL GetAcceptLegalDocumentsUrl() {
  return GetBaseAutocheckoutUrl().Resolve("acceptLegalDocuments");
}

GURL GetSendStatusUrl() {
  return GetBaseAutocheckoutUrl().Resolve("reportStatus");
}

GURL GetSaveToWalletUrl() {
  return GetBaseAutocheckoutUrl().Resolve("saveToWallet");
}

GURL GetPassiveAuthUrl() {
  return GetBaseWalletUrl().Resolve("passiveauth");
}

GURL GetSecureUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string wallet_secure_url =
      command_line.GetSwitchValueASCII(switches::kWalletSecureServiceUrl);
  return !wallet_secure_url.empty() ? GURL(wallet_secure_url) :
                                      GURL(kDefaultWalletSecureServiceUrl);
}

}  // namespace wallet
