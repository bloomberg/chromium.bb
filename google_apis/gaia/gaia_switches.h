// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
#define GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_

namespace switches {

// Supplies custom client login to OAuth2 URL for testing purposes.
extern const char kClientLoginToOAuth2Url[];

// Specifies the path for GAIA authentication URL. The default value is
// "https://accounts.google.com".
extern const char kGaiaUrl[];

// Specifies the backend server used for Google API calls. The https:// prefix
// and the trailing slash should be omitted.
// The default value is "www.googleapis.com".
extern const char kGoogleApisHost[];

// Specifies the backend server used for lso authentication calls.
// "https://accounts.google.com".
extern const char kLsoUrl[];

// TODO(zelidrag): Get rid of all following since all URLs should be
// controlled only with --gaia-host, --lso-host and --google-apis-host.

// Specifies custom OAuth1 login scope for testing purposes.
extern const char kOAuth1LoginScope[];

// Specifies custom OAuth2 issue token URL for testing purposes.
extern const char kOAuth2IssueTokenUrl[];

// Specifies custom OAuth2 token URL for testing purposes.
extern const char kOAuth2TokenUrl[];

// Specifies custom OAuth user info URL for testing purposes.
extern const char kOAuthUserInfoUrl[];
}  // namespace switches

#endif  // GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
