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
class ExtensionPrefs;
class ExtensionServiceInterface;
struct ExtensionSyncData;
struct UninstalledExtensionInfo;

namespace sync_pb {
class ExtensionSpecifics;
}  // sync_pb

namespace browser_sync {

// Returns whether or not the given extension is one we want to sync.
bool IsExtensionValid(const Extension& extension);

// Stringifies the given ExtensionSpecifics.
std::string ExtensionSpecificsToString(
    const sync_pb::ExtensionSpecifics& specifics);

// Returns whether or not the values of the given specifics are valid,
// in particular the id, version, and update URL.
bool IsExtensionSpecificsValid(
    const sync_pb::ExtensionSpecifics& specifics);

// Equivalent to DCHECK(IsExtensionSpecificsValid(specifics)) <<
// ExtensionSpecificsToString(specifics);
void DcheckIsExtensionSpecificsValid(
    const sync_pb::ExtensionSpecifics& specifics);

// Returns true iff two ExtensionSpecifics denote the same extension
// state.  Neither |a| nor |b| need to be valid.
bool AreExtensionSpecificsEqual(const sync_pb::ExtensionSpecifics& a,
                                const sync_pb::ExtensionSpecifics& b);

// Returns true iff the given ExtensionSpecifics is equal to the empty
// ExtensionSpecifics object.  |specifics| does not have to be valid
// and indeed, IsExtensionSpecificsValid(specifics) ->
// !IsExtensionSpecificsUnset(specifics).
bool IsExtensionSpecificsUnset(
    const sync_pb::ExtensionSpecifics& specifics);

// Copies the user properties from |specifics| into |dest_specifics|.
// User properties are properties that are set by the user, i.e. not
// inherent to the extension.  Currently they include |enabled| and
// |incognito_enabled|.  Neither parameter need be valid.
void CopyUserProperties(
    const sync_pb::ExtensionSpecifics& specifics,
    sync_pb::ExtensionSpecifics* dest_specifics);

// Copies everything but non-user properties.  Neither parameter need
// be valid.
void CopyNonUserProperties(
    const sync_pb::ExtensionSpecifics& specifics,
    sync_pb::ExtensionSpecifics* dest_specifics);

// Returns true iff two ExtensionSpecifics have the same user
// properties.  Neither |a| nor |b| need to be valid.
bool AreExtensionSpecificsUserPropertiesEqual(
    const sync_pb::ExtensionSpecifics& a,
    const sync_pb::ExtensionSpecifics& b);

// Returns true iff two ExtensionSpecifics have the same non-user
// properties.  Neither |a| nor |b| need to be valid.
bool AreExtensionSpecificsNonUserPropertiesEqual(
    const sync_pb::ExtensionSpecifics& a,
    const sync_pb::ExtensionSpecifics& b);

// Fills |specifics| with information taken from |extension|, which
// must be a syncable extension.  |specifics| will be valid after this
// function is called.
void GetExtensionSpecifics(const Extension& extension,
                           const ExtensionServiceInterface& extension_service,
                           sync_pb::ExtensionSpecifics* specifics);

// Merge |specifics| into |merged_specifics|.  Both must be valid and
// have the same ID.  The merge policy is currently to copy the
// non-user properties of |specifics| into |merged_specifics| (and the
// user properties if |merge_user_properties| is set) if |specifics|
// has a more recent or the same version as |merged_specifics|.
void MergeExtensionSpecifics(
    const sync_pb::ExtensionSpecifics& specifics,
    bool merge_user_properties,
    sync_pb::ExtensionSpecifics* merged_specifics);

// Fills |sync_data| with the data from |specifics|.  Returns true iff
// succesful.
bool GetExtensionSyncData(
    const sync_pb::ExtensionSpecifics& specifics,
    ExtensionSyncData* sync_data);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_UTIL_H_
