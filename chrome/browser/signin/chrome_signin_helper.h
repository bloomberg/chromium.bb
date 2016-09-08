// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_HELPER_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_HELPER_H_

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

// Adds X-Chrome-Connected header to all Gaia requests from a connected profile,
// with the exception of requests from gaia webview. Must be called on IO
// thread.
// Returns true if the account management header was added to the request.
// Removes X-Chrome-Connected header if it is already in the headers and
// it should not be there.
void FixMirrorRequestHeaderHelper(net::URLRequest* request,
                                  const GURL& redirect_url,
                                  ProfileIOData* io_data,
                                  int child_id,
                                  int route_id);

// Looks for the X-Chrome-Manage-Accounts response header, and if found,
// tries to show the avatar bubble in the browser identified by the
// child/route id. Must be called on IO thread.
void ProcessMirrorResponseHeaderIfExists(net::URLRequest* request,
                                         ProfileIOData* io_data,
                                         int child_id,
                                         int route_id);

};  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_HELPER_H_
