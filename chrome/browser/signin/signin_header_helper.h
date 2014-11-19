// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_HEADER_HELPER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_HEADER_HELPER_H_

#include <string>

namespace net {
class URLRequest;
}
class GURL;
class ProfileIOData;

// Utility functions for handling Chrome/Gaia headers during signin process.
// In the Mirror world, Chrome identity should always stay in sync with Gaia
// identity. Therefore Chrome needs to send Gaia special header for requests
// from a connected profile, so that Gaia can modify its response accordingly
// and let Chrome handles signin with native UI.
namespace signin {

// Profile mode flags.
enum ProfileMode {
  PROFILE_MODE_DEFAULT = 0,
  // Incognito mode disabled by enterprise policy or by parental controls.
  PROFILE_MODE_INCOGNITO_DISABLED = 1 << 0,
  // Adding account disabled in the Android-for-EDU mode.
  PROFILE_MODE_ADD_ACCOUNT_DISABLED = 1 << 1
};

// The ServiceType specified by GAIA in the response header accompanying the 204
// response. This indicates the action Chrome is supposed to lead the user to
// perform.
enum GAIAServiceType {
  GAIA_SERVICE_TYPE_NONE = 0,                 // No GAIA response header.
  GAIA_SERVICE_TYPE_SIGNOUT,                  // Logout all existing sessions.
  GAIA_SERVICE_TYPE_INCOGNITO,                // Open an incognito tab.
  GAIA_SERVICE_TYPE_ADDSESSION,               // Add a secondary account.
  GAIA_SERVICE_TYPE_REAUTH,                   // Re-authenticate an account.
  GAIA_SERVICE_TYPE_SIGNUP,                   // Create a new account.
  GAIA_SERVICE_TYPE_DEFAULT,                  // All other cases.
};

// Struct describing the paramters received in the manage account header.
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
  // The child id associated with the web content of the request.
  int child_id;
  // The route id associated with the web content of the request.
  int route_id;

  ManageAccountsParams();
};

// Adds X-Chrome-Connected header to all Gaia requests from a connected profile,
// with the exception of requests from gaia webview. Must be called on IO
// thread.
// Returns true if the account management header was added to the request.
bool AppendMirrorRequestHeaderIfPossible(
    net::URLRequest* request,
    const GURL& redirect_url,
    ProfileIOData* io_data,
    int child_id,
    int route_id);

// Looks for the X-Chrome-Manage-Accounts response header, and if found,
// tries to show the avatar bubble in the browser identified by the
// child/route id. Must be called on IO thread.
void ProcessMirrorResponseHeaderIfExists(
    net::URLRequest* request,
    ProfileIOData* io_data,
    int child_id,
    int route_id);

};  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_HEADER_HELPER_H_
