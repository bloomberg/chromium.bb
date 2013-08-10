// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_SYNCED_PREF_OBSERVER_H_
#define CHROME_BROWSER_PREFS_SYNCED_PREF_OBSERVER_H_

#include <string>

class SyncedPrefObserver {
 public:
  virtual void OnSyncedPrefChanged(const std::string& path, bool from_sync) = 0;
};

#endif  // CHROME_BROWSER_PREFS_SYNCED_PREF_OBSERVER_H_
