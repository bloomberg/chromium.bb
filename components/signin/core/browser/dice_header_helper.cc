// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_header_helper.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"

namespace signin {

namespace {

const char kActionAttrName[] = "action";
const char kAuthUserAttrName[] = "authuser";
const char kAuthorizationCodeAttrName[] = "authorization_code";
const char kEmailAttrName[] = "email";
const char kIdAttrName[] = "id";

// Determines the Dice action that has been passed from Gaia in the header.
DiceAction GetDiceActionFromHeader(const std::string& value) {
  if (value == "SIGNIN")
    return DiceAction::SIGNIN;
  else if (value == "SIGNOUT")
    return DiceAction::SIGNOUT;
  else if (value == "SINGLE_SESSION_SIGN_OUT")
    return DiceAction::SINGLE_SESSION_SIGNOUT;
  else
    return DiceAction::NONE;
}

}  // namespace

// static
DiceResponseParams DiceHeaderHelper::BuildDiceResponseParams(
    const std::string& header_value) {
  DCHECK(!header_value.empty());
  DiceResponseParams params;
  ResponseHeaderDictionary header_dictionary =
      ParseAccountConsistencyResponseHeader(header_value);
  ResponseHeaderDictionary::const_iterator it = header_dictionary.begin();
  for (; it != header_dictionary.end(); ++it) {
    const std::string key_name(it->first);
    const std::string value(it->second);
    if (key_name == kActionAttrName) {
      params.user_intention = GetDiceActionFromHeader(value);
    } else if (key_name == kIdAttrName) {
      params.obfuscated_gaia_id = value;
    } else if (key_name == kEmailAttrName) {
      params.email = value;
    } else if (key_name == kAuthUserAttrName) {
      bool parse_success = base::StringToInt(value, &params.session_index);
      if (!parse_success)
        params.session_index = -1;
    } else if (key_name == kAuthorizationCodeAttrName) {
      params.authorization_code = value;
    } else {
      DLOG(WARNING) << "Unexpected Gaia header attribute '" << key_name << "'.";
    }
  }

  if (params.obfuscated_gaia_id.empty() || params.email.empty() ||
      params.session_index == -1) {
    DLOG(WARNING) << "Missing parameters for Dice header.";
    params.user_intention = DiceAction::NONE;
  }

  if (params.user_intention == DiceAction::SIGNIN &&
      params.authorization_code.empty()) {
    DLOG(WARNING) << "Missing authorization code for Dice SIGNIN.";
    params.user_intention = DiceAction::NONE;
  }

  return params;
}

bool DiceHeaderHelper::IsUrlEligibleForRequestHeader(const GURL& url) {
  if (switches::GetAccountConsistencyMethod() !=
      switches::AccountConsistencyMethod::kDice) {
    return false;
  }
  return gaia::IsGaiaSignonRealm(url.GetOrigin());
}

std::string DiceHeaderHelper::BuildRequestHeader(const std::string& account_id,
                                                 bool sync_enabled) {
  std::vector<std::string> parts;
  parts.push_back("client_id=" +
                  GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  if (sync_enabled)
    parts.push_back("sync_account_id=" + account_id);
  return base::JoinString(parts, ",");
}

}  // namespace signin
