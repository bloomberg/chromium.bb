// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_UTIL_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_UTIL_H_
#pragma once

// This file contains some low-level utility functions used by
// extensions sync.

#include <string>

class Extension;
struct ExtensionSyncData;

namespace sync_pb {
class ExtensionSpecifics;
}  // sync_pb

namespace browser_sync {

// Returns whether or not the given extension is one we want to sync.
bool IsExtensionValid(const Extension& extension);

// Stringifies the given ExtensionSpecifics.
std::string ExtensionSpecificsToString(
    const sync_pb::ExtensionSpecifics& specifics);

// Fills |sync_data| with the data from |specifics|.  Returns true iff
// succesful.
bool SpecificsToSyncData(
    const sync_pb::ExtensionSpecifics& specifics,
    ExtensionSyncData* sync_data);

// Fills |specifics| with the data from |sync_data|.
void SyncDataToSpecifics(
    const ExtensionSyncData& sync_data,
    sync_pb::ExtensionSpecifics* specifics);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_UTIL_H_
