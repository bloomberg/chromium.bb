// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_CHROME_ACCESS_TOKEN_STORE_H_
#define CHROME_BROWSER_GEOLOCATION_CHROME_ACCESS_TOKEN_STORE_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "content/browser/geolocation/access_token_store.h"

class PrefService;

// Creates a new access token store backed by the global chome prefs.
class ChromeAccessTokenStore : public AccessTokenStore {
 public:
  static void RegisterPrefs(PrefService* prefs);

  ChromeAccessTokenStore();

 private:
  void LoadDictionaryStoreInUIThread(
      scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request);

  // AccessTokenStore
  virtual void DoLoadAccessTokens(
      scoped_refptr<CancelableRequest<LoadAccessTokensCallbackType> > request)
          OVERRIDE;
  virtual void SaveAccessToken(
      const GURL& server_url, const string16& access_token) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeAccessTokenStore);
};

#endif  // CHROME_BROWSER_GEOLOCATION_CHROME_ACCESS_TOKEN_STORE_H_
