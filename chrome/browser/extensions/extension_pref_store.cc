// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_pref_store.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"

ExtensionPrefStore::ExtensionPrefStore(
    ExtensionPrefValueMap* extension_pref_value_map,
    bool incognito_pref_store)
    : extension_pref_value_map_(extension_pref_value_map),
      incognito_pref_store_(incognito_pref_store) {
  extension_pref_value_map_->AddObserver(this);
}

ExtensionPrefStore::~ExtensionPrefStore() {
  if (extension_pref_value_map_)
    extension_pref_value_map_->RemoveObserver(this);
}

void ExtensionPrefStore::OnInitializationCompleted() {
  NotifyInitializationCompleted();
}

void ExtensionPrefStore::OnPrefValueChanged(const std::string& key) {
  CHECK(extension_pref_value_map_);
  const Value *winner =
      extension_pref_value_map_->GetEffectivePrefValue(key,
                                                       incognito_pref_store_);
  if (winner)
    SetValue(key, winner->DeepCopy());
  else
    RemoveValue(key);
}

void ExtensionPrefStore::OnExtensionPrefValueMapDestruction() {
  CHECK(extension_pref_value_map_);
  extension_pref_value_map_->RemoveObserver(this);
  extension_pref_value_map_ = NULL;
}
