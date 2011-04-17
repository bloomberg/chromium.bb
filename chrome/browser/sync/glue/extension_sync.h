// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
class ExtensionServiceInterface;
class Profile;
class ProfileSyncService;

namespace sync_api {
struct UserShare;
}  // namespace sync_api

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
                         sync_api::UserShare* user_share,
                         bool* has_children);

// Fills |extension_data_map| with both client-side information about
// installed extensions and the server-side information about
// extensions to be synced.  Returns true iff this was successful; if
// unsuccessful, the contents of |extension_data_map| are undefined.
bool SlurpExtensionData(const ExtensionSyncTraits& traits,
                        const ExtensionServiceInterface& extensions_service,
                        sync_api::UserShare* user_share,
                        ExtensionDataMap* extension_data_map);

// Updates the server and client as necessary from
// |extension_data_map|.  Returns true iff this was successful.
//
// NOTE(akalin): Keep in mind that updating the client is an
// asynchronous process; the only thing that's guaranteed if this
// function is returned is that the updates were successfully started.
bool FlushExtensionData(const ExtensionSyncTraits& traits,
                        const ExtensionDataMap& extension_data_map,
                        ExtensionServiceInterface* extensions_service,
                        sync_api::UserShare* user_share);

// Updates the server data for the given extension.  Returns true iff
// this was successful; if unsuccessful, an error string is put into
// |error|.
bool UpdateServerData(const ExtensionSyncTraits& traits,
                      const Extension& extension,
                      const ExtensionServiceInterface& extensions_service,
                      sync_api::UserShare* user_share,
                      std::string* error);

// Removes the server data for the given extension ID.
void RemoveServerData(const ExtensionSyncTraits& traits,
                      const std::string& id,
                      sync_api::UserShare* user_share);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SYNC_H_
