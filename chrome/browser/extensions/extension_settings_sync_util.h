// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_SYNC_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_SYNC_UTIL_H_
#pragma once


#include "chrome/browser/sync/api/sync_data.h"
#include "chrome/browser/sync/api/sync_change.h"

namespace base {
class Value;
}  // namespace base

namespace extension_settings_sync_util {

// Creates a SyncData object for an extension setting.
SyncData CreateData(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value);

// Creates an "add" sync change for an extension setting.
SyncChange CreateAdd(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value);

// Creates an "update" sync change for an extension setting.
SyncChange CreateUpdate(
  const std::string& extension_id,
  const std::string& key,
  const base::Value& value);

// Creates a "delete" sync change for an extension setting.
SyncChange CreateDelete(
    const std::string& extension_id, const std::string& key);

}  // namespace extension_settings_sync_util

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_SYNC_UTIL_H_
