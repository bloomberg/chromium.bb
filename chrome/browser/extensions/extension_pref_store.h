// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_STORE_H_
#pragma once

#include "chrome/browser/prefs/value_map_pref_store.h"

// A PrefStore implementation that holds preferences set by extensions.
class ExtensionPrefStore : public ValueMapPrefStore {
 public:
  ExtensionPrefStore();
  virtual ~ExtensionPrefStore() {}

  // Set an extension preference |value| for |key|. Takes ownership of |value|.
  void SetExtensionPref(const std::string& key, Value* value);

  // Remove the extension preference value for |key|.
  void RemoveExtensionPref(const std::string& key);

  // Tell the store it's now fully initialized.
  void OnInitializationCompleted();

 private:
  // PrefStore overrides:
  virtual bool IsInitializationComplete() const;

  bool initialization_complete_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefStore);
};


#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREF_STORE_H_
