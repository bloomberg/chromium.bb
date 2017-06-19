// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_

#include <map>
#include <string>

#include "build/build_config.h"
#include "url/gurl.h"

namespace content_settings {
class CookieSettings;
}

namespace net {
class URLRequest;
}

namespace signin {

// Profile mode flags.
enum ProfileMode {
  PROFILE_MODE_DEFAULT = 0,
  // Incognito mode disabled by enterprise policy or by parental controls.
  PROFILE_MODE_INCOGNITO_DISABLED = 1 << 0,
  // Adding account disabled in the Android-for-EDU mode.
  PROFILE_MODE_ADD_ACCOUNT_DISABLED = 1 << 1
};

extern const char kChromeConnectedHeader[];
extern const char kDiceRequestHeader[];

// The ServiceType specified by Gaia in the response header accompanying the 204
// response. This indicates the action Chrome is supposed to lead the user to
// perform.
enum GAIAServiceType {
  GAIA_SERVICE_TYPE_NONE = 0,    // No Gaia response header.
  GAIA_SERVICE_TYPE_SIGNOUT,     // Logout all existing sessions.
  GAIA_SERVICE_TYPE_INCOGNITO,   // Open an incognito tab.
  GAIA_SERVICE_TYPE_ADDSESSION,  // Add a secondary account.
  GAIA_SERVICE_TYPE_REAUTH,      // Re-authenticate an account.
  GAIA_SERVICE_TYPE_SIGNUP,      // Create a new account.
  GAIA_SERVICE_TYPE_DEFAULT,     // All other cases.
};

enum class DiceAction {
  NONE,
  SIGNIN,                 // Sign in an account.
  SIGNOUT,                // Sign out of all sessions.
  SINGLE_SESSION_SIGNOUT  // Sign out of a single session.
};

// Struct describing the parameters received in the manage account header.
struct ManageAccountsParams {
  // The requested service type such as "ADDSESSION".
  GAIAServiceType service_type;
  // The prefilled email.
  std::string email;
  // Whether |email| is a saml account.
  bool is_saml;
  // The continue URL after the requested service is completed successfully.
  // Defaults to the current URL if empty.
  std::string continue_url;
  // Whether the continue URL should be loaded in the same tab.
  bool is_same_tab;

// iOS has no notion of route and child IDs.
#if !defined(OS_IOS)
  // The child ID associated with the web content of the request.
  int child_id;
  // The route ID associated with the web content of the request.
  int route_id;
#endif  // !defined(OS_IOS)

  ManageAccountsParams();
  ManageAccountsParams(const ManageAccountsParams& other);
};

// Struct describing the parameters received in the Dice response header.
struct DiceResponseParams {
  DiceResponseParams();
  ~DiceResponseParams();
  DiceResponseParams(const DiceResponseParams& other);

  DiceAction user_intention;

  // Gaia ID of the account signed in or signed out (which may be a secondary
  // account). When |user_intention| is SIGNOUT, this is the ID of the primary
  // account.
  std::string obfuscated_gaia_id;

  // Email of the account signed in or signed out. When |user_intention| is
  // SIGNOUT, this is the email of the primary account.
  std::string email;

  // Session index for the account signed in or signed out. When
  // |user_intention| is SIGNOUT, this is 0.
  int session_index;

  // Optional. Must be set when |user_intention| is SIGNIN.
  std::string authorization_code;
};

// Base class for managing the signin headers (Dice and Chrome-Connected).
class SigninHeaderHelper {
 public:
  // Appends or remove the header to a network request if necessary.
  bool AppendOrRemoveRequestHeader(net::URLRequest* request,
                                   const GURL& redirect_url,
                                   const char* header_name,
                                   const std::string& header_value);

  // Returns wether an account consistency header should be built for this
  // request.
  bool ShouldBuildRequestHeader(
      const GURL& url,
      const content_settings::CookieSettings* cookie_settings);

 protected:
  SigninHeaderHelper() {}
  virtual ~SigninHeaderHelper() {}

  // Dictionary of fields in a account consistency response header.
  using ResponseHeaderDictionary = std::map<std::string, std::string>;

  // Parses the account consistency response header. Its expected format is
  // "key1=value1,key2=value2,...".
  static ResponseHeaderDictionary ParseAccountConsistencyResponseHeader(
      const std::string& header_value);

 private:
  // Returns whether the url is eligible for the request header.
  virtual bool IsUrlEligibleForRequestHeader(const GURL& url) = 0;
};

// Returns true if signin cookies are allowed.
bool SettingsAllowSigninCookies(
    const content_settings::CookieSettings* cookie_settings);

// Returns the CHROME_CONNECTED cookie, or an empty string if it should not be
// added to the request to |url|.
std::string BuildMirrorRequestCookieIfPossible(
    const GURL& url,
    const std::string& account_id,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask);

// Adds account consistency header to all Gaia requests from a connected
// profile, with the exception of requests from gaia webview.
// Removes the header in case it should not be transfered to a redirected url.
void AppendOrRemoveAccountConsistentyRequestHeader(
    net::URLRequest* request,
    const GURL& redirect_url,
    const std::string& account_id,
    bool sync_enabled,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask);

// Returns the parameters contained in the X-Chrome-Manage-Accounts response
// header.
ManageAccountsParams BuildManageAccountsParams(const std::string& header_value);

#if !defined(OS_IOS) && !defined(OS_ANDROID)
// Returns the parameters contained in the X-Chrome-ID-Consistency-Response
// response header.
// Returns DiceAction::NONE in case of error (such as missing or malformed
// parameters).
DiceResponseParams BuildDiceResponseParams(const std::string& header_value);
#endif

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_HEADER_HELPER_H_
