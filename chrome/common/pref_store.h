// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PREF_STORE_H_
#define CHROME_COMMON_PREF_STORE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

class Value;

// This is an abstract interface for reading and writing from/to a persistent
// preference store, used by PrefService. An implementation using a JSON file
// can be found in JsonPrefStore, while an implementation without any backing
// store for testing can be found in TestingPrefStore. Furthermore, there is
// CommandLinePrefStore, which bridges command line options to preferences and
// ConfigurationPolicyPrefStore, which is used for hooking up configuration
// policy with the preference subsystem.
class PrefStore : public base::RefCounted<PrefStore> {
 public:
  // Observer interface for monitoring PrefStore.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the value for the given |key| in the store changes.
    virtual void OnPrefValueChanged(const std::string& key) = 0;
    // Notification about the PrefStore being fully initialized.
    virtual void OnInitializationCompleted() = 0;
  };

  // Return values for GetValue().
  enum ReadResult {
    // Value found and returned.
    READ_OK,
    // No value present, but skip other pref stores and use default.
    READ_USE_DEFAULT,
    // No value present.
    READ_NO_VALUE,
  };

  PrefStore() {}

  // Add and remove observers.
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}

  // Whether the store has completed all asynchronous initialization.
  virtual bool IsInitializationComplete() const;

  // Get the value for a given preference |key| and stores it in |result|.
  // |result| is only modified if the return value is READ_OK. Ownership of the
  // |result| value remains with the PrefStore.
  virtual ReadResult GetValue(const std::string& key,
                              const Value** result) const = 0;

 protected:
  friend class base::RefCounted<PrefStore>;
  virtual ~PrefStore() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefStore);
};

#endif  // CHROME_COMMON_PREF_STORE_H_
