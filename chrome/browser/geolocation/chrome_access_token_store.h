// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_CHROME_ACCESS_TOKEN_STORE_H_
#define CHROME_BROWSER_GEOLOCATION_CHROME_ACCESS_TOKEN_STORE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/geolocation/access_token_store.h"

class PrefRegistrySimple;

// Creates a new access token store backed by the global chome prefs.
class ChromeAccessTokenStore : public device::AccessTokenStore {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  ChromeAccessTokenStore();

  void LoadAccessTokens(const LoadAccessTokensCallback& request) override;

 private:
  ~ChromeAccessTokenStore() override;

  // AccessTokenStore
  void SaveAccessToken(const GURL& server_url,
                       const base::string16& access_token) override;

  DISALLOW_COPY_AND_ASSIGN(ChromeAccessTokenStore);
};

#endif  // CHROME_BROWSER_GEOLOCATION_CHROME_ACCESS_TOKEN_STORE_H_
