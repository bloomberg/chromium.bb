// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
#define GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_

namespace switches {

// Supplies custom client login to OAuth2 URL for testing purposes.
extern const char kClientLoginToOAuth2Url[];

// Specifies the backend server used for gaia authentications, like sync or
// policies for example. The https:// prefix and the trailing slash should be
// omitted. The default value is "www.google.com".
extern const char kGaiaHost[];

// Specifies the path prefix for GAIA authentication URL. It should be used
// for testing in cases where authentication path prefix differs from the one
// used in production.
extern const char kGaiaUrlPath[];

// Specifies custom OAuth1 login scope for testing purposes.
extern const char kOAuth1LoginScope[];

// Specifies custom OAuth2 issue token URL for testing purposes.
extern const char kOAuth2IssueTokenUrl[];

// Specifies custom OAuth2 token URL for testing purposes.
extern const char kOAuth2TokenUrl[];

}  // namespace switches

#endif  // GOOGLE_APIS_GAIA_GAIA_SWITCHES_H_
