// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_SYNC_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_SYNC_H_
#pragma once

// This file contains functions necessary for syncing
// extensions-related data types.

#include <map>
#include <string>

class Extension;
class ExtensionService;
class Profile;
class ProfileSyncService;

namespace sync_pb {
class ExtensionSpecifics;
}  // namespace sync_pb

namespace browser_sync {

class ExtensionData;
struct ExtensionSyncTraits;

// A map from extension IDs to ExtensionData objects.
typedef std::map<std::string, ExtensionData> ExtensionDataMap;

// Fills in |has_children| with whether or not the root node with the
// given tag has child nodes.  Returns true iff the lookup succeeded.
//
// TODO(akalin): Move this somewhere where it can be used by
// other data types.
bool RootNodeHasChildren(const char* tag,
                         ProfileSyncService* sync_service,
                         bool* has_children);

ExtensionService* GetExtensionServiceFromProfile(Profile* profile);

// Fills |extension_data_map| with both client-side information about
// installed extensions and the server-side information about
// extensions to be synced.  Returns true iff this was successful; if
// unsuccessful, the contents of |extension_data_map| are undefined.
bool SlurpExtensionData(const ExtensionSyncTraits& traits,
                        ProfileSyncService* sync_service,
                        ExtensionDataMap* extension_data_map);

// Updates the server and client as necessary from
// |extension_data_map|.  Returns true iff this was successful.
//
// NOTE(akalin): Keep in mind that updating the client is an
// asynchronous process; the only thing that's guaranteed if this
// function is returned is that the updates were successfully started.
bool FlushExtensionData(const ExtensionSyncTraits& traits,
                        const ExtensionDataMap& extension_data_map,
                        ProfileSyncService* sync_service);

// Updates the server data for the given extension.  Returns true iff
// this was successful; if unsuccessful, an error string is put into
// |error|.
bool UpdateServerData(const ExtensionSyncTraits& traits,
                      const Extension& extension,
                      ProfileSyncService* sync_service,
                      std::string* error);

// Removes the server data for the given extension ID.
void RemoveServerData(const ExtensionSyncTraits& traits,
                      const std::string& id,
                      ProfileSyncService* sync_service);

// Starts updating the client data from the given server data.
void UpdateClient(const ExtensionSyncTraits& traits,
                  const sync_pb::ExtensionSpecifics& server_data,
                  ExtensionService* extensions_service);

// Removes existing client data for the given extension.
void RemoveFromClient(const ExtensionSyncTraits& traits,
                      const std::string& id,
                      ExtensionService* extensions_service);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SYNC_H_
