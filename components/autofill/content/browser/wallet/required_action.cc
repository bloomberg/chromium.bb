// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/required_action.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace autofill {
namespace wallet {

bool ActionAppliesToFullWallet(RequiredAction action) {
  return action == UPDATE_EXPIRATION_DATE ||
         action == VERIFY_CVV ||
         action == CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS ||
         action == REQUIRE_PHONE_NUMBER;
}

bool ActionAppliesToSaveToWallet(RequiredAction action) {
  return action == INVALID_FORM_FIELD ||
         action == REQUIRE_PHONE_NUMBER;
}

bool ActionAppliesToWalletItems(RequiredAction action) {
  return action == SETUP_WALLET ||
         action == CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS ||
         action == ACCEPT_TOS ||
         action == GAIA_AUTH ||
         action == REQUIRE_PHONE_NUMBER ||
         action == UPDATE_EXPIRATION_DATE ||
         action == UPGRADE_MIN_ADDRESS ||
         action == PASSIVE_GAIA_AUTH;
}

RequiredAction ParseRequiredActionFromString(const std::string& str) {
  std::string str_lower;
  base::TrimWhitespaceASCII(StringToLowerASCII(str), base::TRIM_ALL,
                            &str_lower);

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
  else if (str_lower == "require_phone_number")
    return REQUIRE_PHONE_NUMBER;
  else if (str_lower == "choose_another_instrument_or_address")
    return CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS;

  DLOG(ERROR) << "Failed to parse: \"" << str << "\" as a required action";
  return UNKNOWN_TYPE;
}

}  // namespace wallet
}  // namespace autofill
