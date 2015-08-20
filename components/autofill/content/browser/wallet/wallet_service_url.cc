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

const char kSandboxWalletSecureServiceUrl[] =
    "https://wallet-web.sandbox.google.com/";

bool IsWalletProductionEnabled() {
  // If the command line flag exists, it takes precedence.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
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

GURL GetBaseSecureUrl() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string wallet_secure_url =
      command_line.GetSwitchValueASCII(switches::kWalletSecureServiceUrl);
  if (!wallet_secure_url.empty())
    return GURL(wallet_secure_url);
  if (IsWalletProductionEnabled())
    return GURL(kProdWalletServiceUrl);
  return GURL(kSandboxWalletSecureServiceUrl);
}

}  // namespace

namespace wallet {

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

}  // namespace wallet
}  // namespace autofill
