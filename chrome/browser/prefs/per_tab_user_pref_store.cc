// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/per_tab_user_pref_store.h"
#include "chrome/common/pref_names.h"

PerTabUserPrefStore::PerTabUserPrefStore(PersistentPrefStore* underlay)
    : OverlayUserPrefStore(underlay) {
  RegisterOverlayProperty(
      prefs::kWebKitJavascriptEnabled, prefs::kWebKitGlobalJavascriptEnabled);
}
