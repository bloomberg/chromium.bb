// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_data.h"

#include "base/logging.h"

ExtensionSyncData::ExtensionSyncData()
    : uninstalled(false), enabled(false), incognito_enabled(false) {}

ExtensionSyncData::~ExtensionSyncData() {}

void ExtensionSyncData::Merge(const ExtensionSyncData& new_data) {
  CHECK_EQ(id, new_data.id);
  CHECK(!uninstalled);
  CHECK(!new_data.uninstalled);

  // Copy version-independent properties.
  enabled = new_data.enabled;
  incognito_enabled = new_data.incognito_enabled;

  // Copy version-dependent properties if version <= new_data.version.
  if (version.CompareTo(new_data.version) <= 0) {
    version = new_data.version;
    update_url = new_data.update_url;
    name = new_data.name;
  }
}
