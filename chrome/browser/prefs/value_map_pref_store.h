// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_VALUE_MAP_PREF_STORE_H_
#define CHROME_BROWSER_PREFS_VALUE_MAP_PREF_STORE_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "chrome/common/pref_store.h"

// A basic PrefStore implementation that uses a simple name-value map for
// storing the preference values.
class ValueMapPrefStore : public PrefStore {
 public:
  ValueMapPrefStore();
  virtual ~ValueMapPrefStore();

  // PrefStore overrides:
  virtual ReadResult GetValue(const std::string& key, Value** value) const;
  virtual void AddObserver(PrefStore::Observer* observer);
  virtual void RemoveObserver(PrefStore::Observer* observer);

 protected:
  // Store a |value| for |key| in the store. Also generates an notification if
  // the value changed. Assumes ownership of |value|, which must be non-NULL.
  void SetValue(const std::string& key, Value* value);

  // Remove the value for |key| from the store. Sends a notification if there
  // was a value to be removed.
  void RemoveValue(const std::string& key);

  // Notify observers about the initialization completed event.
  void NotifyInitializationCompleted();

 private:
  PrefValueMap prefs_;

  ObserverList<PrefStore::Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ValueMapPrefStore);
};

#endif  // CHROME_BROWSER_PREFS_VALUE_MAP_PREF_STORE_H_
