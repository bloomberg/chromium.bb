// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_service_url.h"

#include <string>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace autofill {
namespace {

const char kProdWalletServiceUrl[] = "https://wallet.google.com/";

const char kSandboxWalletServiceUrl[] =
    "https://wallet-web.sandbox.google.com/";

const char kSandboxWalletSecureServiceUrl[] =
    "https://wallet-web.sandbox.google.com/";

bool IsWalletProductionEnabled() {
  // If the command line flag exists, it takes precedence.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  std::string sandbox_enabled(
      command_line->GetSwitchValueASCII(switches::kWalletServiceUseSandbox));
  if (!sandbox_enabled.empty())
    return sandbox_enabled != "1";

  // Default to sandbox when --reduce-security-for-testing is passed to allow
  // rAc on http:// pages.
  if (command_line->HasSwitch(::switches::kReduceSecurityForTesting))
    return false;

#if defined(ENABLE_PROD_WALLET_SERVICE)
  return true;
#else
  return false;
#endif
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

GURL GetBaseWalletUrl(size_t user_index) {
  std::string path = base::StringPrintf("online/v2/u/%" PRIuS "/", user_index);
  return GetWalletHostUrl().Resolve(path);
}

GURL GetBaseAutocheckoutUrl(size_t user_index) {
  return GetBaseWalletUrl(user_index).Resolve("wallet/autocheckout/v1/");
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

GURL GetBaseEncryptedFrontendUrl(size_t user_index) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  GURL base_url = IsWalletProductionEnabled() ||
      command_line.HasSwitch(switches::kWalletServiceUrl) ?
          GetWalletHostUrl() : GetBaseSecureUrl();
  std::string path =
      base::StringPrintf("online-secure/v2/u/%" PRIuS "/autocheckout/v1/",
                         user_index);
  return base_url.Resolve(path);
}

}  // namespace

namespace wallet {

GURL GetGetWalletItemsUrl(size_t user_index) {
  return GetBaseAutocheckoutUrl(user_index).Resolve("getWalletItemsJwtless");
}

GURL GetGetFullWalletUrl(size_t user_index) {
  return GetBaseEncryptedFrontendUrl(user_index)
      .Resolve("getFullWalletJwtless?s7e=otp");
}

GURL GetManageInstrumentsUrl(size_t user_index) {
  std::string path =
      base::StringPrintf("manage/w/%" PRIuS "/paymentMethods", user_index);
  return GetBaseSecureUrl().Resolve(path);
}

GURL GetManageAddressesUrl(size_t user_index) {
  std::string path =
      base::StringPrintf("manage/w/%" PRIuS "/settings/addresses", user_index);
  return GetBaseSecureUrl().Resolve(path);
}

GURL GetAcceptLegalDocumentsUrl(size_t user_index) {
  return GetBaseAutocheckoutUrl(user_index).Resolve("acceptLegalDocument");
}

GURL GetAuthenticateInstrumentUrl(size_t user_index) {
  return GetBaseEncryptedFrontendUrl(user_index)
      .Resolve("authenticateInstrument?s7e=cvn");
}

GURL GetSendStatusUrl(size_t user_index) {
  return GetBaseAutocheckoutUrl(user_index).Resolve("reportStatus");
}

GURL GetSaveToWalletNoEscrowUrl(size_t user_index) {
  return GetBaseAutocheckoutUrl(user_index).Resolve("saveToWallet");
}

GURL GetSaveToWalletUrl(size_t user_index) {
  return GetBaseEncryptedFrontendUrl(user_index)
      .Resolve("saveToWallet?s7e=card_number%3Bcvn");
}

GURL GetPassiveAuthUrl(size_t user_index) {
  return GetBaseWalletUrl(user_index)
      .Resolve("passiveauth?isChromePayments=true");
}

GURL GetSignInUrl() {
  GURL url(GaiaUrls::GetInstance()->add_account_url());
  url = net::AppendQueryParameter(url, "nui", "1");
  // Prevents promos from showing (see http://crbug.com/235227).
  url = net::AppendQueryParameter(url, "sarp", "1");
  url = net::AppendQueryParameter(url,
                                  "continue",
                                  GetSignInContinueUrl().spec());
  return url;
}

// The continue url portion of the sign-in URL. This URL is used as a milestone
// to determine that the sign-in process is finished. It has to be a Google
// domain, use https://, and do almost nothing, but otherwise it's not too
// important what the URL actually is: it's not important that this URL has the
// ability to generate a gdToken.
GURL GetSignInContinueUrl() {
  return GetPassiveAuthUrl(0);
}

bool IsSignInContinueUrl(const GURL& url, size_t* user_index) {
  GURL final_url = GetSignInContinueUrl();
  if (url.scheme() != final_url.scheme() ||
      url.host() != final_url.host() ||
      url.path() != final_url.path()) {
    return false;
  }

  *user_index = 0;
  std::string query_str = url.query();
  url::Component query(0, query_str.length());
  url::Component key, value;
  const char kUserIndexKey[] = "authuser";
  while (url::ExtractQueryKeyValue(query_str.c_str(), &query, &key, &value)) {
    if (key.is_nonempty() &&
        query_str.substr(key.begin, key.len) == kUserIndexKey) {
      base::StringToSizeT(query_str.substr(value.begin, value.len), user_index);
      break;
    }
  }

  return true;
}

bool IsSignInRelatedUrl(const GURL& url) {
  size_t unused;
  return url.GetOrigin() == GetSignInUrl().GetOrigin() ||
      StartsWith(base::UTF8ToUTF16(url.GetOrigin().host()),
                 base::ASCIIToUTF16("accounts."),
                 false) ||
      IsSignInContinueUrl(url, &unused);
}

bool IsUsingProd() {
  return GetWalletHostUrl() == GURL(kProdWalletServiceUrl);
}

}  // namespace wallet
}  // namespace autofill
