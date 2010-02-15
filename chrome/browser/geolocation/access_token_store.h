// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_ACCESS_TOKEN_STORE_H_
#define CHROME_BROWSER_GEOLOCATION_ACCESS_TOKEN_STORE_H_

#include "base/ref_counted.h"
#include "base/string16.h"

class GURL;
class PrefService;

// Provides storage for the access token used in the network request.
// Provided to allow for mocking for testing, by decoupling deep browser
// dependencies (singleton prefs & threads) from the geolocaiton object.
class AccessTokenStore : public base::RefCounted<AccessTokenStore> {
 public:
  static void RegisterPrefs(PrefService* prefs);

  virtual bool SetAccessToken(const GURL& url,
                              const string16& access_token) = 0;
  virtual bool GetAccessToken(const GURL& url, string16* access_token) = 0;

 protected:
  friend class base::RefCounted<AccessTokenStore>;
  virtual ~AccessTokenStore() {}
};

// Creates a new access token store backed by the global chome prefs.
AccessTokenStore* NewChromePrefsAccessTokenStore();

#endif  // CHROME_BROWSER_GEOLOCATION_ACCESS_TOKEN_STORE_H_
