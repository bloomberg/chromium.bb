// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/wallet_service_url.h"

#include <string>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"

namespace {

const char kDefaultWalletServiceUrl[] = "https://wallet.google.com/";

GURL GetWalletHostUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string wallet_service_hostname =
      command_line.GetSwitchValueASCII(switches::kWalletServiceUrl);
  return !wallet_service_hostname.empty() ? GURL(wallet_service_hostname) :
                                            GURL(kDefaultWalletServiceUrl);
}

GURL GetBaseWalletUrl() {
  return GetWalletHostUrl().Resolve("online/v2/");
}

GURL GetBaseAutocheckoutUrl() {
  return GetBaseWalletUrl().Resolve("wallet/autocheckout/");
}

GURL GetBaseSecureUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string wallet_secure_url =
      command_line.GetSwitchValueASCII(switches::kWalletSecureServiceUrl);
  return !wallet_secure_url.empty() ? GURL(wallet_secure_url) :
                                      GURL(kDefaultWalletServiceUrl);
}

}  // anonymous namespace

namespace wallet {

// TODO(ahutter): This shouldn't live in this class. See
// http://crbug.com/164281.
const char kApiKey[] = "abcdefg";

GURL GetGetWalletItemsUrl() {
  return GetBaseAutocheckoutUrl().Resolve("getWalletItemsJwtless");
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

GURL GetEncryptionUrl() {
  return GetWalletHostUrl().Resolve("online-secure/temporarydata/cvv?s7e=cvv");
}

GURL GetEscrowUrl() {
  return GetBaseSecureUrl().Resolve("dehEfe?s7e=cardNumber%3Bcvv");
}

}  // namespace wallet
