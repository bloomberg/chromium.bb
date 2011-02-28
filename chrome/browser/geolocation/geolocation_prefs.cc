// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_prefs.h"

#include "content/browser/geolocation/access_token_store.h"

namespace geolocation {
void RegisterPrefs(PrefService* prefs) {
  // Fan out to all geolocation sub-components that use prefs.
  AccessTokenStore::RegisterPrefs(prefs);
}
}  // namespace geolocation
