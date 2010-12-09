// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_pref_store.h"

ExtensionPrefStore::ExtensionPrefStore()
    : initialization_complete_(false) {
}

void ExtensionPrefStore::SetExtensionPref(const std::string& key,
                                          Value* value) {
  SetValue(key, value);
}

void ExtensionPrefStore::RemoveExtensionPref(const std::string& key) {
  RemoveValue(key);
}

void ExtensionPrefStore::OnInitializationCompleted() {
  DCHECK(!initialization_complete_);
  initialization_complete_ = true;
  NotifyInitializationCompleted();
}

bool ExtensionPrefStore::IsInitializationComplete() const {
  return initialization_complete_;
}
