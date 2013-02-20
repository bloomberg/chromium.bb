// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/required_action.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace autofill {
namespace wallet {

bool ActionAppliesToFullWallet(RequiredAction action) {
  return action == UPDATE_EXPIRATION_DATE ||
         action == UPGRADE_MIN_ADDRESS ||
         action == INVALID_FORM_FIELD ||
         action == VERIFY_CVV;
}

bool ActionAppliesToWalletItems(RequiredAction action) {
  return action == SETUP_WALLET ||
         action == ACCEPT_TOS ||
         action == GAIA_AUTH ||
         action == INVALID_FORM_FIELD ||
         action == PASSIVE_GAIA_AUTH;
}

RequiredAction ParseRequiredActionFromString(const std::string& str) {
  std::string str_lower;
  TrimWhitespaceASCII(StringToLowerASCII(str), TRIM_ALL, &str_lower);

  if (str_lower == "setup_wallet")
    return SETUP_WALLET;
  else if (str_lower == "accept_tos")
    return ACCEPT_TOS;
  else if (str_lower == "gaia_auth")
    return GAIA_AUTH;
  else if (str_lower == "update_expiration_date")
    return UPDATE_EXPIRATION_DATE;
  else if (str_lower == "upgrade_min_address")
    return UPGRADE_MIN_ADDRESS;
  else if (str_lower == "invalid_form_field")
    return INVALID_FORM_FIELD;
  else if (str_lower == "verify_cvv")
    return VERIFY_CVV;
  else if (str_lower == "passive_gaia_auth")
    return PASSIVE_GAIA_AUTH;

  DLOG(ERROR) << "Failed to parse: \"" << str << "\" as a required action";
  return UNKNOWN_TYPE;
}

}  // namespace wallet
}  // namespace autofill
