// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_REVOCATION_CONSUMER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_REVOCATION_CONSUMER_H_

#include <string>

class GoogleServiceAuthError;

// An interface that defines the callbacks for consumers to which
// OAuth2RevocationFetcher can return results.
class OAuth2RevocationConsumer {
 public:
  virtual ~OAuth2RevocationConsumer() {}

  virtual void OnRevocationSuccess() {}
  virtual void OnRevocationFailure(const GoogleServiceAuthError& error) {}
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_REVOCATION_CONSUMER_H_
