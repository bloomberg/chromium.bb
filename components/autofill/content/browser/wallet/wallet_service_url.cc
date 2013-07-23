// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_service_url.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace autofill {
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
      base::FieldTrialList::FindFullName("WalletProductionService") == "Yes" ||
      base::FieldTrialList::FindFullName("Autocheckout") == "Yes";
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

GURL GetBaseEncryptedFrontendUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // TODO(ahutter): Stop checking these switches once we switch over to prod.
  GURL base_url = IsWalletProductionEnabled() ||
      command_line.HasSwitch(switches::kWalletServiceUrl) ?
          GetWalletHostUrl() : GetBaseSecureUrl();
  return base_url.Resolve("online-secure/v2/autocheckout/v1/");
}

}  // namespace

namespace wallet {

GURL GetGetWalletItemsUrl() {
  return GetBaseAutocheckoutUrl().Resolve("getWalletItemsJwtless");
}

GURL GetGetFullWalletUrl() {
  return GetBaseEncryptedFrontendUrl().Resolve("getFullWalletJwtless?s7e=otp");
}

GURL GetManageInstrumentsUrl() {
  return GetBaseSecureUrl().Resolve("manage/paymentMethods");
}

GURL GetManageAddressesUrl() {
  return GetBaseSecureUrl().Resolve("manage/settings/addresses");
}

GURL GetAcceptLegalDocumentsUrl() {
  return GetBaseAutocheckoutUrl().Resolve("acceptLegalDocument");
}

GURL GetAuthenticateInstrumentUrl() {
  return GetBaseEncryptedFrontendUrl()
      .Resolve("authenticateInstrument?s7e=cvn");
}

GURL GetSendStatusUrl() {
  return GetBaseAutocheckoutUrl().Resolve("reportStatus");
}

GURL GetSaveToWalletNoEscrowUrl() {
  return GetBaseAutocheckoutUrl().Resolve("saveToWallet");
}

GURL GetSaveToWalletUrl() {
  return GetBaseEncryptedFrontendUrl()
      .Resolve("saveToWallet?s7e=card_number%3Bcvn");
}

GURL GetPassiveAuthUrl() {
  return GetBaseWalletUrl().Resolve("passiveauth?isChromePayments=true");
}

GURL GetSignInUrl() {
  GURL url(GaiaUrls::GetInstance()->service_login_url());
  url = net::AppendQueryParameter(url, "service", "toolbar");
  url = net::AppendQueryParameter(url, "nui", "1");
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

bool IsUsingProd() {
  return GetWalletHostUrl() == GURL(kProdWalletServiceUrl);
}

}  // namespace wallet
}  // namespace autofill
