// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_AUTH_ARC_AUTH_FETCHER_H_
#define COMPONENTS_ARC_AUTH_ARC_AUTH_FETCHER_H_

#include <string>

#include "base/macros.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

class GURL;

namespace net {
class URLRequestContextGetter;
}

namespace arc {

// Fetches ARC auth code. Fetching process starts automatically on creation.
class ArcAuthFetcher : public GaiaAuthConsumer {
 public:
  // Returns a result of ARC auth code fetching.
  class Delegate {
   public:
    // Called when code was fetched.
    virtual void OnAuthCodeFetched(const std::string& code) = 0;
    // Called when additional UI authorization is required.
    virtual void OnAuthCodeNeedUI() = 0;
    // Called when auth code cannot be received.
    virtual void OnAuthCodeFailed() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  ArcAuthFetcher(net::URLRequestContextGetter* getter, Delegate* delegate);
  ~ArcAuthFetcher() override = default;

  // GaiaAuthConsumer:
  void OnClientOAuthCode(const std::string& auth_code) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

  // Helper function to compose target URL for current user, also is used in
  // test.
  static GURL CreateURL();

 private:
  void FetchAuthCode();

  // Unowned pointer.
  Delegate* const delegate_;

  GaiaAuthFetcher auth_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthFetcher);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_AUTH_ARC_AUTH_FETCHER_H_
