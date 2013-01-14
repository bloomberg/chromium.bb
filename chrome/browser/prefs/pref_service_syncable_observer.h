// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_OBSERVER_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_OBSERVER_H_

class PrefServiceSyncableObserver {
 public:
  // Invoked when PrefService::IsSyncing() changes.
  virtual void OnIsSyncingChanged() = 0;

 protected:
  virtual ~PrefServiceSyncableObserver() {}
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_OBSERVER_H_
