// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_user_pref_store.h"
#include "chrome/common/pref_names.h"

IncognitoUserPrefStore::IncognitoUserPrefStore(PersistentPrefStore* underlay)
    : OverlayUserPrefStore(underlay) {
  // List of keys that cannot be changed in the user prefs file by the incognito
  // profile.  All preferences that store information about the browsing history
  // or behavior of the user should have this property.
  RegisterOverlayProperty(prefs::kBrowserWindowPlacement);
}
