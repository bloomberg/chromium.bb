// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_INTERNALS_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_INTERNALS_UTIL_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace signin_internals_util {

// Preference prefixes for signin and token values.
extern const char kSigninPrefPrefix[];
extern const char kTokenPrefPrefix[];

// The following tokens are not under the purview of TokenService.
extern const char kChromeToMobileToken[];
extern const char kOperationsBaseToken[];
extern const char kUserPolicySigninServiceToken[];
extern const char kProfileDownloaderToken[];
extern const char kObfuscatedGaiaIdFetcherToken[];
extern const char kOAuth2MintTokenFlowToken[];
extern const char* kTokenPrefsArray[];
extern const size_t kNumTokenPrefs;

// Helper enums to access fields from SigninStatus (declared below).
enum {
  SIGNIN_FIELDS_BEGIN = 0,
  UNTIMED_FIELDS_BEGIN = SIGNIN_FIELDS_BEGIN
};

enum UntimedSigninStatusField {
  USERNAME = UNTIMED_FIELDS_BEGIN,
  SID,
  LSID,
  UNTIMED_FIELDS_END
};

enum {
  UNTIMED_FIELDS_COUNT = UNTIMED_FIELDS_END - UNTIMED_FIELDS_BEGIN,
  TIMED_FIELDS_BEGIN = UNTIMED_FIELDS_END
};

enum TimedSigninStatusField {
  SIGNIN_TYPE = TIMED_FIELDS_BEGIN,
  CLIENT_LOGIN_STATUS,
  OAUTH_LOGIN_STATUS,
  GET_USER_INFO_STATUS,
  TIMED_FIELDS_END
};

enum {
  TIMED_FIELDS_COUNT = TIMED_FIELDS_END - TIMED_FIELDS_BEGIN,
  SIGNIN_FIELDS_END = TIMED_FIELDS_END,
  SIGNIN_FIELDS_COUNT = SIGNIN_FIELDS_END - SIGNIN_FIELDS_BEGIN
};

// Encapsulates diagnostic information about tokens for different services.
// Note that although SigninStatus contains a map of service names to token
// values, we replicate the service name within this struct for a cleaner
// serialization (with ToValue()).
struct TokenInfo {
  std::string token;  // The actual token.
  std::string status;  // Status of the last token fetch.
  std::string time;  // Timestamp of the last token fetch.
  std::string service;  // The service that this token is for.

  TokenInfo(const std::string& token,
            const std::string& status,
            const std::string& time,
            const std::string& service);
  TokenInfo();
  ~TokenInfo();

  DictionaryValue* ToValue();
};

// Associates a service name with its token information.
typedef std::map<std::string, TokenInfo> TokenInfoMap;

// Returns the root preference path for the service. The path should be
// qualified with one of .value, .status or .time to get the respective
// full preference path names.
std::string TokenPrefPath(const std::string& service_name);

// Many values in SigninStatus are also associated with a timestamp.
// This makes it easier to keep values and their assoicated times together.
typedef std::pair<std::string, std::string> TimedSigninStatusValue;

// Returns the name of a SigninStatus field.
std::string SigninStatusFieldToString(UntimedSigninStatusField field);
std::string SigninStatusFieldToString(TimedSigninStatusField field);

// Encapsulates both authentication and token related information. Used
// by SigninInternals to maintain information that needs to be shown in
// the about:signin-internals page.
struct SigninStatus {
  std::vector<std::string> untimed_signin_fields;
  std::vector<TimedSigninStatusValue> timed_signin_fields;
  TokenInfoMap token_info_map;

  SigninStatus();
  ~SigninStatus();

  // Returns a dictionary with the following form:
  // { "signin_info" :
  //     [ {"title": "Basic Information",
  //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
  //       },
  //       { "title": "Detailed Information",
  //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
  //       }],
  //   "token_info" :
  //     [ List of {"name": "foo-name", "token" : "foo-token",
  //                 "status": "foo_stat", "time" : "foo_time"} elems]
  //  }
  scoped_ptr<DictionaryValue> ToValue();
};

// An Observer class for authentication and token diagnostic information.
class SigninDiagnosticsObserver {
 public:
  // Credentials and signin related changes.
  virtual void NotifySigninValueChanged(const UntimedSigninStatusField& field,
                                        const std::string& value) {}
  virtual void NotifySigninValueChanged(const TimedSigninStatusField& field,
                                        const std::string& value) {}

  // Tokens and service related changes.
  virtual void NotifyTokenReceivedSuccess(const std::string& token_name,
                                          const std::string& token,
                                          bool update_time) {}
  virtual void NotifyTokenReceivedFailure(const std::string& token_name,
                                          const std::string& error) {}
  virtual void NotifyClearStoredToken(const std::string& token_name) {}

};

} // namespace

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_INTERNALS_UTIL_H_
