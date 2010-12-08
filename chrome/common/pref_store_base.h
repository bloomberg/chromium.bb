// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PREF_STORE_BASE_H_
#define CHROME_COMMON_PREF_STORE_BASE_H_
#pragma once

#include "base/observer_list.h"
#include "chrome/common/pref_store.h"

// Implements the observer-related bits of the PrefStore interface. This is
// provided as a convenience for PrefStore implementations to derive from.
class PrefStoreBase : public PrefStore {
 public:
  virtual ~PrefStoreBase() {}

  // Overriden from PrefStore.
  virtual void AddObserver(ObserverInterface* observer);
  virtual void RemoveObserver(ObserverInterface* observer);

 protected:
  // Send a notification about a changed preference value.
  virtual void NotifyPrefValueChanged(const std::string& key);

  // Notify observers about initialization being complete.
  virtual void NotifyInitializationCompleted();

 private:
  ObserverList<ObserverInterface, true> observers_;
};

#endif  // CHROME_COMMON_PREF_STORE_BASE_H_
