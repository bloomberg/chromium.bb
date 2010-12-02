// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_PREF_VALUE_STORE_H_
#define CHROME_TEST_TESTING_PREF_VALUE_STORE_H_
#pragma once

#include "chrome/browser/prefs/pref_value_store.h"

// PrefValueStore subclass that allows directly setting PrefStores.
class TestingPrefValueStore : public PrefValueStore {
 public:
  TestingPrefValueStore(PrefStore* managed_platform_prefs,
                        PrefStore* device_management_prefs,
                        PrefStore* extension_prefs,
                        PrefStore* command_line_prefs,
                        PrefStore* user_prefs,
                        PrefStore* recommended_prefs,
                        PrefStore* default_prefs)
      : PrefValueStore(managed_platform_prefs, device_management_prefs,
                       extension_prefs, command_line_prefs,
                       user_prefs, recommended_prefs, default_prefs, NULL) {}
};

#endif  // CHROME_TEST_TESTING_PREF_VALUE_STORE_H_
