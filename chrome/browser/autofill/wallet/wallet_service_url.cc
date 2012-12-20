// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/wallet_service_url.h"

#include <string>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"

namespace {

const char kDefaultWalletServiceUrl[] =
    "https://wallet.google.com/online/v2/wallet/autocheckout/";

const char kDefaultWalletSecureServiceUrl[] =
    "https://wallet.google.com/online-secure/temporarydata/cvv?s7e=cvv";

GURL GetBaseWalletUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string base_wallet_service_url =
      command_line.GetSwitchValueASCII(switches::kWalletServiceUrl);
  return !base_wallet_service_url.empty() ? GURL(base_wallet_service_url) :
                                            GURL(kDefaultWalletServiceUrl);
}

}  // anonymous namespace

// TODO(ahutter): This shouldn't live in this class. See
// http://crbug.com/164281.
const char wallet::kApiKey[] = "abcdefg";

GURL wallet::GetGetWalletItemsUrl() {
  return GetBaseWalletUrl().Resolve("getWalletItemsJwtless");
}

GURL wallet::GetGetFullWalletUrl() {
  return GetBaseWalletUrl().Resolve("getFullWalletJwtless");
}

GURL wallet::GetAcceptLegalDocumentsUrl() {
  return GetBaseWalletUrl().Resolve("acceptLegalDocuments");
}

GURL wallet::GetSendStatusUrl() {
  return GetBaseWalletUrl().Resolve("reportStatus");
}

GURL wallet::GetSaveToWalletUrl() {
  return GetBaseWalletUrl().Resolve("saveToWallet");
}

GURL wallet::GetSecureUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string wallet_secure_url =
      command_line.GetSwitchValueASCII(switches::kWalletSecureServiceUrl);
  return !wallet_secure_url.empty() ? GURL(wallet_secure_url) :
                                      GURL(kDefaultWalletSecureServiceUrl);
}

