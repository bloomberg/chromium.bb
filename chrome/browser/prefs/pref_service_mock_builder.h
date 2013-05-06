// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_BUILDER_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_BUILDER_H_

#include "chrome/browser/prefs/pref_service_syncable_builder.h"

class PrefService;
class PrefServiceSyncable;

// A helper that allows convenient building of custom PrefServices in tests.
class PrefServiceMockBuilder : public PrefServiceSyncableBuilder {
 public:
  PrefServiceMockBuilder();
  virtual ~PrefServiceMockBuilder();

  // Creates a PrefService for testing, invalidating the entire
  // builder configuration.
  virtual PrefService* Create(PrefRegistry* pref_registry) OVERRIDE;
  virtual PrefServiceSyncable* CreateSyncable(
      user_prefs::PrefRegistrySyncable* pref_registry) OVERRIDE;

 private:
  virtual void ResetDefaultState() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceMockBuilder);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_BUILDER_H_
