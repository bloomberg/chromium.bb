// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/wallet/wallet_service_url.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/autofill/common/autofill_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "googleurl/src/gurl.h"
#include "net/base/url_util.h"

namespace {

const char kProdWalletServiceUrl[] = "https://wallet.google.com/";

// TODO(ahutter): Remove this once production is ready.
const char kSandboxWalletServiceUrl[] =
    "https://payments-form-dogfood.sandbox.google.com/";

// TODO(ahutter): Remove this once production is ready.
const char kSandboxWalletSecureServiceUrl[] =
    "https://wallet-web.sandbox.google.com/";

bool IsWalletProductionEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kWalletServiceUseProd) ||
         base::FieldTrialList::FindFullName("WalletProductionService") == "Yes";
}

GURL GetWalletHostUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string wallet_service_hostname =
      command_line.GetSwitchValueASCII(switches::kWalletServiceUrl);
  if (!wallet_service_hostname.empty())
    return GURL(wallet_service_hostname);
  if (IsWalletProductionEnabled())
    return GURL(kProdWalletServiceUrl);
  return GURL(kSandboxWalletServiceUrl);
}

GURL GetBaseWalletUrl() {
  return GetWalletHostUrl().Resolve("online/v2/");
}

GURL GetBaseAutocheckoutUrl() {
  return GetBaseWalletUrl().Resolve("wallet/autocheckout/v1/");
}

GURL GetBaseSecureUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string wallet_secure_url =
      command_line.GetSwitchValueASCII(switches::kWalletSecureServiceUrl);
  if (!wallet_secure_url.empty())
    return GURL(wallet_secure_url);
  if (IsWalletProductionEnabled())
    return GURL(kProdWalletServiceUrl);
  return GURL(kSandboxWalletSecureServiceUrl);
}

}  // anonymous namespace

namespace autofill {
namespace wallet {

GURL GetGetWalletItemsUrl() {
  return GetBaseAutocheckoutUrl().Resolve("getWalletItemsJwtless");
}

GURL GetGetFullWalletUrl() {
  return GetBaseAutocheckoutUrl().Resolve("getFullWalletJwtless");
}

GURL GetAcceptLegalDocumentsUrl() {
  return GetBaseAutocheckoutUrl().Resolve("acceptLegalDocument");
}

GURL GetAuthenticateInstrumentUrl() {
  return GetBaseAutocheckoutUrl().Resolve("authenticateInstrument");
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
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // TODO(ahutter): Stop checking these switches once we switch over to prod.
  if (IsWalletProductionEnabled() ||
      command_line.HasSwitch(switches::kWalletServiceUrl)) {
    return GetWalletHostUrl().Resolve(
        "online-secure/temporarydata/cvv?s7e=cvv");
  } else {
    return GetBaseSecureUrl().Resolve(
        "online-secure/temporarydata/cvv?s7e=cvv");
  }
}

GURL GetEscrowUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // TODO(ahutter): Stop checking these switches once we switch over to prod.
  if (IsWalletProductionEnabled() ||
      command_line.HasSwitch(switches::kWalletServiceUrl)) {
    return GetBaseSecureUrl().Resolve("dehEfe?s7e=cardNumber%3Bcvv");
  } else {
    return GetBaseSecureUrl().Resolve("checkout/dehEfe?s7e=cardNumber%3Bcvv");
  }
}

GURL GetSignInUrl() {
  GURL url(GaiaUrls::GetInstance()->service_login_url());
  url = net::AppendQueryParameter(url, "service", "sierra");
  url = net::AppendQueryParameter(url, "btmpl", "popup");
  url = net::AppendQueryParameter(url,
                                  "continue",
                                  GetSignInContinueUrl().spec());
  return url;
}

// The continue url portion of the sign-in URL.
GURL GetSignInContinueUrl() {
  return GetPassiveAuthUrl();
}

bool IsSignInContinueUrl(const GURL& url) {
  GURL final_url = wallet::GetSignInContinueUrl();
  return url.SchemeIsSecure() &&
         url.host() == final_url.host() &&
         url.path() == final_url.path();
}

}  // namespace wallet
}  // namespace autofill
