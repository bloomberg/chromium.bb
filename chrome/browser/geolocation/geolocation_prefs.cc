// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_prefs.h"

#include "chrome/browser/geolocation/chrome_access_token_store.h"

namespace geolocation {
void RegisterPrefs(PrefRegistrySimple* registry) {
  // Fan out to all geolocation sub-components that use prefs.
  ChromeAccessTokenStore::RegisterPrefs(registry);
}

}  // namespace geolocation
