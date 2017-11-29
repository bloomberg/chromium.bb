// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_header_helper.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"

namespace signin {

const char kDiceProtocolVersion[] = "1";

namespace {

// Request parameters.
const char kRequestSigninAll[] = "all_accounts";
const char kRequestSigninSyncAccount[] = "sync_account";
const char kRequestSignoutNoConfirmation[] = "no_confirmation";
const char kRequestSignoutShowConfirmation[] = "show_confirmation";

// Signin response parameters.
const char kSigninActionAttrName[] = "action";
const char kSigninAuthUserAttrName[] = "authuser";
const char kSigninAuthorizationCodeAttrName[] = "authorization_code";
const char kSigninEmailAttrName[] = "email";
const char kSigninIdAttrName[] = "id";

// Signout response parameters.
const char kSignoutEmailAttrName[] = "email";
const char kSignoutSessionIndexAttrName[] = "sessionindex";
const char kSignoutObfuscatedIDAttrName[] = "obfuscatedid";

// Determines the Dice action that has been passed from Gaia in the header.
DiceAction GetDiceActionFromHeader(const std::string& value) {
  if (value == "SIGNIN")
    return DiceAction::SIGNIN;
  else if (value == "SIGNOUT")
    return DiceAction::SIGNOUT;
  else if (value == "ENABLE_SYNC")
    return DiceAction::ENABLE_SYNC;
  else
    return DiceAction::NONE;
}

}  // namespace

DiceHeaderHelper::DiceHeaderHelper(bool signed_in_with_auth_error)
    : signed_in_with_auth_error_(signed_in_with_auth_error) {}

// static
DiceResponseParams DiceHeaderHelper::BuildDiceSigninResponseParams(
    const std::string& header_value) {
  DCHECK(!header_value.empty());
  DiceResponseParams params;
  ResponseHeaderDictionary header_dictionary =
      ParseAccountConsistencyResponseHeader(header_value);
  if (header_dictionary.count(kSigninActionAttrName) != 1u)
    return params;

  DiceResponseParams::AccountInfo* info = nullptr;
  switch (GetDiceActionFromHeader(
      header_dictionary.find(kSigninActionAttrName)->second)) {
    case DiceAction::NONE:
    case DiceAction::SIGNOUT:
      DLOG(WARNING) << "Only SIGNIN and ENABLE_SYNC are supported through "
                    << "X-Chrome-ID-Consistency-Response :" << header_value;
      return params;
    case DiceAction::SIGNIN:
      params.user_intention = DiceAction::SIGNIN;
      params.signin_info = std::make_unique<DiceResponseParams::SigninInfo>();
      info = &params.signin_info->account_info;
      break;
    case DiceAction::ENABLE_SYNC:
      params.user_intention = DiceAction::ENABLE_SYNC;
      params.enable_sync_info =
          std::make_unique<DiceResponseParams::EnableSyncInfo>();
      info = &params.enable_sync_info->account_info;
      break;
  }

  ResponseHeaderDictionary::const_iterator it = header_dictionary.begin();
  for (; it != header_dictionary.end(); ++it) {
    const std::string key_name(it->first);
    const std::string value(it->second);
    if (key_name == kSigninActionAttrName) {
      // Do nothing, this was already parsed.
    } else if (key_name == kSigninIdAttrName) {
      info->gaia_id = value;
    } else if (key_name == kSigninEmailAttrName) {
      info->email = value;
    } else if (key_name == kSigninAuthUserAttrName) {
      bool parse_success = base::StringToInt(value, &info->session_index);
      if (!parse_success)
        info->session_index = -1;
    } else if (key_name == kSigninAuthorizationCodeAttrName) {
      if (params.signin_info)
        params.signin_info->authorization_code = value;
      else
        DLOG(WARNING) << "Authorization code expected only with SIGNIN action";
    } else {
      DLOG(WARNING) << "Unexpected Gaia header attribute '" << key_name << "'.";
    }
  }

  if (info->gaia_id.empty() || info->email.empty() ||
      info->session_index == -1) {
    DLOG(WARNING) << "Missing account info in Dice header: " << header_value;
    return DiceResponseParams();
  }

  if (params.signin_info && params.signin_info->authorization_code.empty()) {
    DLOG(WARNING) << "Missing authorization code in Dice SIGNIN header: "
                  << header_value;
    return DiceResponseParams();
  }

  return params;
}

// static
DiceResponseParams DiceHeaderHelper::BuildDiceSignoutResponseParams(
    const std::string& header_value) {
  // Google internal documentation of this header at:
  // http://go/gaia-response-headers
  DCHECK(!header_value.empty());
  DiceResponseParams params;
  params.user_intention = DiceAction::SIGNOUT;
  std::vector<std::string> gaia_ids;
  std::vector<std::string> emails;
  std::vector<int> session_indices;
  ResponseHeaderDictionary header_dictionary =
      ParseAccountConsistencyResponseHeader(header_value);
  ResponseHeaderDictionary::const_iterator it = header_dictionary.begin();
  for (; it != header_dictionary.end(); ++it) {
    const std::string key_name(it->first);
    const std::string value(it->second);
    if (key_name == kSignoutObfuscatedIDAttrName) {
      gaia_ids.push_back(value);
      // The Gaia ID is wrapped in quotes.
      base::TrimString(value, "\"", &gaia_ids.back());
    } else if (key_name == kSignoutEmailAttrName) {
      // The email is wrapped in quotes.
      emails.push_back(value);
      base::TrimString(value, "\"", &emails.back());
    } else if (key_name == kSignoutSessionIndexAttrName) {
      int session_index = -1;
      bool parse_success = base::StringToInt(value, &session_index);
      if (parse_success)
        session_indices.push_back(session_index);
    } else {
      DLOG(WARNING) << "Unexpected Gaia header attribute '" << key_name << "'.";
    }
  }

  if ((gaia_ids.size() != emails.size()) ||
      (gaia_ids.size() != session_indices.size())) {
    DLOG(WARNING) << "Invalid parameter count for Dice SIGNOUT header: "
                  << header_value;
    return DiceResponseParams();
  }

  if (gaia_ids.empty()) {
    DLOG(WARNING) << "No account specified in Dice SIGNOUT header";
    return DiceResponseParams();
  }

  params.signout_info = std::make_unique<DiceResponseParams::SignoutInfo>();
  for (size_t i = 0; i < gaia_ids.size(); ++i) {
    params.signout_info->account_infos.emplace_back(gaia_ids[i], emails[i],
                                                    session_indices[i]);
  }

  return params;
}

bool DiceHeaderHelper::IsUrlEligibleForRequestHeader(const GURL& url) {
  if (!IsDiceFixAuthErrorsEnabled())
    return false;

  // With kDiceFixAuthError, only set the request header if the user is signed
  // in and has an authentication error.
  if (!signed_in_with_auth_error_ &&
      (GetAccountConsistencyMethod() ==
       AccountConsistencyMethod::kDiceFixAuthErrors)) {
    return false;
  }

  return gaia::IsGaiaSignonRealm(url.GetOrigin());
}

std::string DiceHeaderHelper::BuildRequestHeader(
    const std::string& sync_account_id,
    SignoutMode signout_mode) {
  // When fixing auth errors, only add the header when Sync is actually in error
  // state.
  DCHECK(signed_in_with_auth_error_ ||
         (GetAccountConsistencyMethod() !=
          AccountConsistencyMethod::kDiceFixAuthErrors));
  DCHECK(!(sync_account_id.empty() && signed_in_with_auth_error_));

  std::vector<std::string> parts;
  parts.push_back(base::StringPrintf("version=%s", kDiceProtocolVersion));
  parts.push_back("client_id=" +
                  GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  if (!sync_account_id.empty())
    parts.push_back("sync_account_id=" + sync_account_id);

  // Restrict Signin to Sync account only when fixing auth errors.
  std::string signin_mode = (GetAccountConsistencyMethod() ==
                             AccountConsistencyMethod::kDiceFixAuthErrors)
                                ? kRequestSigninSyncAccount
                                : kRequestSigninAll;
  parts.push_back("signin_mode=" + signin_mode);

  // Show the signout confirmation only when Dice is fully enabled.
  const char* signout_mode_value = nullptr;
  switch (signout_mode) {
    case SignoutMode::kNoSignoutConfirmation:
      signout_mode_value = kRequestSignoutNoConfirmation;
      break;
    case SignoutMode::kShowSignoutConfirmation:
      signout_mode_value = kRequestSignoutShowConfirmation;
      break;
  }
  DCHECK(signout_mode_value);
  parts.push_back(base::StringPrintf("signout_mode=%s", signout_mode_value));

  return base::JoinString(parts, ",");
}

}  // namespace signin
