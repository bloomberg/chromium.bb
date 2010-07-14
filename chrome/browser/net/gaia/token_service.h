// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The TokenService will supply authentication tokens for any service that
// needs it. One such service is Sync.
// For the time being, the Token Service just supplies the LSID from the
// ChromiumOS login. In the future, it'll have a bit more logic and supply
// only AuthTokens from Gaia, and not LSIDs.

#ifndef CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_H_
#define CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_H_

#include "chrome/common/net/gaia/gaia_auth_consumer.h"

class TokenService {
 public:
  void SetClientLoginResult(
    const GaiaAuthConsumer::ClientLoginResult& credentials);

  bool HasLsid();
  const std::string& GetLsid();

  // TODO(chron): Token broadcast will require removing a lot of auth code
  //              from sync. For the time being we'll start with LSID passing.

 private:
  GaiaAuthConsumer::ClientLoginResult credentials_;
};

#endif  // CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_H_
