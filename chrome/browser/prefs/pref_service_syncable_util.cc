// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_syncable_util.h"

#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"

PrefServiceSyncable* PrefServiceSyncableFromProfile(Profile* profile) {
  return static_cast<PrefServiceSyncable*>(profile->GetPrefs());
}

PrefServiceSyncable* PrefServiceSyncableIncognitoFromProfile(Profile* profile) {
  return static_cast<PrefServiceSyncable*>(profile->GetOffTheRecordPrefs());
}
