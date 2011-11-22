// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PER_TAB_USER_PREF_STORE_H_
#define CHROME_BROWSER_PREFS_PER_TAB_USER_PREF_STORE_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/prefs/overlay_user_pref_store.h"

class PerTabUserPrefStore : public OverlayUserPrefStore {
 public:
  explicit PerTabUserPrefStore(PersistentPrefStore* underlay);

 private:
  DISALLOW_COPY_AND_ASSIGN(PerTabUserPrefStore);
};

#endif  // CHROME_BROWSER_PREFS_PER_TAB_USER_PREF_STORE_H_
