// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_CHROME_ACCESS_TOKEN_STORE_H_
#define CHROME_BROWSER_GEOLOCATION_CHROME_ACCESS_TOKEN_STORE_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/access_token_store.h"

class PrefRegistrySimple;

// Creates a new access token store backed by the global chome prefs.
class ChromeAccessTokenStore : public content::AccessTokenStore {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  ChromeAccessTokenStore();

  virtual void LoadAccessTokens(
      const LoadAccessTokensCallbackType& request) OVERRIDE;

 private:
  virtual ~ChromeAccessTokenStore();

  // AccessTokenStore
  virtual void SaveAccessToken(
      const GURL& server_url, const base::string16& access_token) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeAccessTokenStore);
};

#endif  // CHROME_BROWSER_GEOLOCATION_CHROME_ACCESS_TOKEN_STORE_H_
