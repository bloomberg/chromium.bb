// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PREF_STORE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PREF_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/pref_value_map.h"

// A light-weight prefstore implementation that keeps preferences
// in a memory backed store. This is not a persistent prefstore -- we
// subclass the PersistentPrefStore here since it is needed by the
// PrefService, which in turn is needed by the Autofill component.
class AwPrefStore : public PersistentPrefStore {
 public:
  AwPrefStore();

  // Overriden from PrefStore.
  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const override;
  virtual void AddObserver(PrefStore::Observer* observer) override;
  virtual void RemoveObserver(PrefStore::Observer* observer) override;
  virtual bool HasObservers() const override;
  virtual bool IsInitializationComplete() const override;

  // PersistentPrefStore overrides:
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) override;
  virtual void ReportValueChanged(const std::string& key) override;
  virtual void SetValue(const std::string& key, base::Value* value) override;
  virtual void SetValueSilently(const std::string& key,
                                base::Value* value) override;
  virtual void RemoveValue(const std::string& key) override;
  virtual bool ReadOnly() const override;
  virtual PrefReadError GetReadError() const override;
  virtual PersistentPrefStore::PrefReadError ReadPrefs() override;
  virtual void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  virtual void CommitPendingWrite() override {}

 protected:
  virtual ~AwPrefStore();

 private:
  // Stores the preference values.
  PrefValueMap prefs_;

  ObserverList<PrefStore::Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(AwPrefStore);
};

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PREF_STORE_H_
