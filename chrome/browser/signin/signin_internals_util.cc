// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_internals_util.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_constants.h"

namespace signin_internals_util {

const char kSigninPrefPrefix[] = "google.services.signin.";
const char kTokenPrefPrefix[] = "google.services.signin.tokens.";
const char kChromeToMobileToken[] = "ChromeToMobile";
const char kAppNotifyChannelSetupToken[] = "AppNotifyChannelSetup";
const char kOperationsBaseToken[] = "OperationsBase";
const char kUserPolicySigninServiceToken[] = "UserCloudPolicyManagerToken";
const char kProfileDownloaderToken[] = "ProfileDownloader";

// TODO(vishwath): These two services need their information plumbed to
// about:signin-internals.
const char kObfuscatedGaiaIdFetcherToken[] = "ObfuscatedGaiaIdFetcher";
const char kOAuth2MintTokenFlowToken[] = "OAuth2MintTokenFlow";
const char* kTokenPrefsArray[] = {
  GaiaConstants::kSyncService,
  GaiaConstants::kLSOService,
  GaiaConstants::kGaiaOAuth2LoginRefreshToken,
  GaiaConstants::kGaiaOAuth2LoginAccessToken,
  kChromeToMobileToken,
  kAppNotifyChannelSetupToken,
  kOperationsBaseToken,
  kUserPolicySigninServiceToken,
  kProfileDownloaderToken,
  kObfuscatedGaiaIdFetcherToken,
  kOAuth2MintTokenFlowToken
};

const size_t kNumTokenPrefs = arraysize(kTokenPrefsArray);


namespace {

// Gets the first 6 hex characters of the SHA256 hash of the passed in string.
// These are enough to perform equality checks across a single users tokens,
// while preventing outsiders from reverse-engineering the actual token from
// the displayed value.
// Note that for readability (in about:signin-internals), an empty string
// is not hashed, but simply returned as an empty string.
const size_t kTruncateSize = 3;
std::string GetTruncatedHash(const std::string& str) {
  if (str.empty())
    return str;

  char hash_val[kTruncateSize];
  crypto::SHA256HashString(str, &hash_val[0], kTruncateSize);
  return StringToLowerASCII(base::HexEncode(&hash_val[0], kTruncateSize));
}

} // namespace

TokenInfo::TokenInfo(const std::string& token,
                     const std::string& status,
                     const std::string& time,
                     const std::string& service)
    : token(token),
      status(status),
      time(time),
      service(service) {
}

TokenInfo::TokenInfo() {
}

TokenInfo::~TokenInfo() {
}

DictionaryValue* TokenInfo::ToValue() {
  scoped_ptr<DictionaryValue> token_info(new DictionaryValue());
  token_info->SetString("service", service);
  token_info->SetString("token", GetTruncatedHash(token));
  token_info->SetString("status", status);
  token_info->SetString("time", time);

  return token_info.release();
}

#define ENUM_CASE(x) case x: return (std::string(kSigninPrefPrefix) + #x)
std::string SigninStatusFieldToString(UntimedSigninStatusField field) {
  switch (field) {
    ENUM_CASE(USERNAME);
    ENUM_CASE(SID);
    ENUM_CASE(LSID);
    case UNTIMED_FIELDS_END:
      NOTREACHED();
      return "";
  }

  NOTREACHED();
  return "";
}

std::string SigninStatusFieldToString(TimedSigninStatusField field) {
  switch (field) {
    ENUM_CASE(SIGNIN_TYPE);
    ENUM_CASE(CLIENT_LOGIN_STATUS);
    ENUM_CASE(OAUTH_LOGIN_STATUS);
    ENUM_CASE(GET_USER_INFO_STATUS);
    case TIMED_FIELDS_END:
      NOTREACHED();
      return "";
  }

  NOTREACHED();
  return "";
}

SigninStatus::SigninStatus()
    :untimed_signin_fields(UNTIMED_FIELDS_COUNT),
     timed_signin_fields(TIMED_FIELDS_COUNT) {
}

SigninStatus::~SigninStatus() {
}

std::string TokenPrefPath(const std::string& token_name) {
  return std::string(kTokenPrefPrefix) + token_name;
}

namespace {

ListValue* AddSection(ListValue* parent_list, const std::string& title) {
  scoped_ptr<DictionaryValue> section(new DictionaryValue());
  ListValue* section_contents = new ListValue();

  section->SetString("title", title);
  section->Set("data", section_contents);
  parent_list->Append(section.release());
  return section_contents;
}

void AddSectionEntry(ListValue* section_list,
                     const std::string& field_name,
                     const std::string& field_val) {
  scoped_ptr<DictionaryValue> entry(new DictionaryValue());
  entry->SetString("label", field_name);
  entry->SetString("value", field_val);
  section_list->Append(entry.release());
}


void AddSectionEntry(ListValue* section_list,
                     const std::string& field_name,
                     uint32 field_val) {
  std::stringstream ss;
  ss << field_val;
  scoped_ptr<DictionaryValue> entry(new DictionaryValue());
  entry->SetString("label", field_name);
  entry->SetString("value", ss.str());
  section_list->Append(entry.release());
}

// Returns a string describing the chrome version environment. Version format:
// <Build Info> <OS> <Version number> (<Last change>)<channel or "-devel">
// If version information is unavailable, returns "invalid."
std::string GetVersionString() {
  // Build a version string that matches MakeUserAgentForSyncApi with the
  // addition of channel info and proper OS names.
  chrome::VersionInfo chrome_version;
  if (!chrome_version.is_valid())
    return "invalid";
  // GetVersionStringModifier returns empty string for stable channel or
  // unofficial builds, the channel string otherwise. We want to have "-devel"
  // for unofficial builds only.
  std::string version_modifier =
      chrome::VersionInfo::GetVersionStringModifier();
  if (version_modifier.empty()) {
    if (chrome::VersionInfo::GetChannel() !=
            chrome::VersionInfo::CHANNEL_STABLE) {
      version_modifier = "-devel";
    }
  } else {
    version_modifier = " " + version_modifier;
  }
  return chrome_version.Name() + " " + chrome_version.OSType() + " " +
      chrome_version.Version() + " (" + chrome_version.LastChange() + ")" +
      version_modifier;
}


std::string SigninStatusFieldToLabel(UntimedSigninStatusField field) {
  switch (field) {
    case USERNAME:
      return "User Id";
    case LSID:
      return "Lsid (Hash)";
    case SID:
      return "Sid (Hash)";
    case UNTIMED_FIELDS_END:
      NOTREACHED();
      return "";
  }
  NOTREACHED();
  return "";
}

TimedSigninStatusValue SigninStatusFieldToLabel(
    TimedSigninStatusField field) {
  switch (field) {
    case SIGNIN_TYPE:
      return TimedSigninStatusValue("Type", "Time");
    case CLIENT_LOGIN_STATUS:
      return TimedSigninStatusValue("Last OnClientLogin Status",
                                    "Last OnClientLogin Time");
    case OAUTH_LOGIN_STATUS:
      return TimedSigninStatusValue("Last OnOAuthLogin Status",
                                    "Last OnOAuthLogin Time");

    case GET_USER_INFO_STATUS:
      return TimedSigninStatusValue("Last OnGetUserInfo Status",
                                    "Last OnGetUserInfo Time");
    case TIMED_FIELDS_END:
      NOTREACHED();
      return TimedSigninStatusValue("Error", "");
  }
  NOTREACHED();
  return TimedSigninStatusValue("Error", "");
}

} //  namespace

scoped_ptr<DictionaryValue> SigninStatus::ToValue() {
  scoped_ptr<DictionaryValue> signin_status(new DictionaryValue());
  ListValue* signin_info = new ListValue();
  signin_status->Set("signin_info", signin_info);

  // A summary of signin related info first.
  ListValue* basic_info = AddSection(signin_info, "Basic Information");
  const std::string signin_status_string =
      untimed_signin_fields[USERNAME - UNTIMED_FIELDS_BEGIN].empty() ?
      "Not Signed In" : "Signed In";
  AddSectionEntry(basic_info, "Chrome Version", GetVersionString());
  AddSectionEntry(basic_info, "Signin Status", signin_status_string);
  int i;
  for (i = UNTIMED_FIELDS_BEGIN; i < UNTIMED_FIELDS_END; ++i) {
    const std::string field =
        SigninStatusFieldToLabel(static_cast<UntimedSigninStatusField>(i));
    if (i == SID || i == LSID) {
      AddSectionEntry(
          basic_info,
          field,
          GetTruncatedHash(untimed_signin_fields[i - UNTIMED_FIELDS_BEGIN]));
    } else {
      AddSectionEntry(
          basic_info,
          field,
          untimed_signin_fields[i - UNTIMED_FIELDS_BEGIN]);
    }
  }

  // Time and status information of the possible sign in types.
  ListValue* detailed_info = AddSection(signin_info, "Last Signin Details");
  for (; i < TIMED_FIELDS_END; ++i) {
    const std::string value_field =
        SigninStatusFieldToLabel(static_cast<TimedSigninStatusField>(i)).first;
    const std::string time_field =
        SigninStatusFieldToLabel(static_cast<TimedSigninStatusField>(i)).second;

    AddSectionEntry(detailed_info, value_field,
                    timed_signin_fields[i - TIMED_FIELDS_BEGIN].first);
    AddSectionEntry(detailed_info, time_field,
                    timed_signin_fields[i - TIMED_FIELDS_BEGIN].second);
  }

  // Token information for all services.
  ListValue* token_info = new ListValue();
  ListValue* token_details = AddSection(token_info, "Token Details");
  signin_status->Set("token_info", token_info);
  for (std::map<std::string, TokenInfo>::iterator it = token_info_map.begin();
       it != token_info_map.end(); ++it) {
    DictionaryValue* token_info = it->second.ToValue();
    token_details->Append(token_info);
  }

  return signin_status.Pass();
}

} //  namespace signin_internals_util
