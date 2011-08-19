// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_sync.h"

#include <utility>

#include "base/logging.h"
#include "base/tracked.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/sync/glue/extension_sync_traits.h"
#include "chrome/browser/sync/glue/extension_util.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"

namespace browser_sync {

bool RootNodeHasChildren(const char* tag,
                         sync_api::UserShare* user_share,
                         bool* has_children) {
  CHECK(has_children);
  *has_children = false;
  sync_api::ReadTransaction trans(FROM_HERE, user_share);
  sync_api::ReadNode node(&trans);
  if (!node.InitByTagLookup(tag)) {
    LOG(ERROR) << "Root node with tag " << tag << " does not exist";
    return false;
  }
  *has_children = node.GetFirstChildId() != sync_api::kInvalidId;
  return true;
}

namespace {

// Fills in |extension_data_map| with data from
// extension_service.GetSyncDataList().
void SlurpClientData(
    IsValidAndSyncablePredicate is_valid_and_syncable,
    const ExtensionServiceInterface& extension_service,
    ExtensionDataMap* extension_data_map) {
  std::vector<ExtensionSyncData> sync_data_list =
      extension_service.GetSyncDataList(is_valid_and_syncable);
  for (std::vector<ExtensionSyncData>::const_iterator it =
           sync_data_list.begin();
       it != sync_data_list.end(); ++it) {
    std::pair<ExtensionDataMap::iterator, bool> result =
        extension_data_map->insert(std::make_pair(it->id, *it));
    if (!result.second) {
      // The value wasn't inserted, so merge it in.
      result.first->second.Merge(*it);
    }
  }
}

// Gets the boilerplate error message for not being able to find a
// root node.
//
// TODO(akalin): Put this somewhere where all data types can use it.
std::string GetRootNodeDoesNotExistError(const char* root_node_tag) {
  return
      std::string("Server did not create the top-level ") +
      root_node_tag +
      " node. We might be running against an out-of-date server.";
}

// Gets the data from the server for extensions to be synced and
// updates |extension_data_map|.
bool SlurpServerData(
    const char* root_node_tag,
    const ExtensionSpecificsGetter extension_specifics_getter,
    sync_api::UserShare* user_share,
    ExtensionDataMap* extension_data_map) {
  sync_api::WriteTransaction trans(FROM_HERE, user_share);
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(root_node_tag)) {
    LOG(ERROR) << GetRootNodeDoesNotExistError(root_node_tag);
    return false;
  }

  int64 id = root.GetFirstChildId();
  while (id != sync_api::kInvalidId) {
    sync_api::ReadNode sync_node(&trans);
    if (!sync_node.InitByIdLookup(id)) {
      LOG(ERROR) << "Failed to fetch sync node for id " << id;
      return false;
    }
    const sync_pb::ExtensionSpecifics& server_data =
        (*extension_specifics_getter)(sync_node);
    ExtensionSyncData sync_data;
    if (!SpecificsToSyncData(server_data, &sync_data)) {
      LOG(ERROR) << "Invalid extensions specifics for id " << id;
      return false;
    }
    (*extension_data_map)[sync_data.id] = sync_data;
    id = sync_node.GetSuccessorId();
  }
  return true;
}

}  // namespace

bool SlurpExtensionData(const ExtensionSyncTraits& traits,
                        const ExtensionServiceInterface& extension_service,
                        sync_api::UserShare* user_share,
                        ExtensionDataMap* extension_data_map) {
  // Read server-side data first so client user settings take
  // precedence.
  if (!SlurpServerData(
          traits.root_node_tag, traits.extension_specifics_getter,
          user_share, extension_data_map)) {
    return false;
  }

  SlurpClientData(
      traits.is_valid_and_syncable, extension_service,
      extension_data_map);
  return true;
}

namespace {

// Updates the server data from the given extension data.  Returns
// whether or not the update was successful.
bool UpdateServer(
    const ExtensionSyncTraits& traits,
    const ExtensionSyncData& data,
    sync_api::WriteTransaction* trans) {
  sync_pb::ExtensionSpecifics specifics;
  SyncDataToSpecifics(data, &specifics);
  const std::string& id = data.id;
  sync_api::WriteNode write_node(trans);
  if (write_node.InitByClientTagLookup(traits.model_type, id)) {
    (*traits.extension_specifics_setter)(specifics, &write_node);
  } else {
    sync_api::ReadNode root(trans);
    if (!root.InitByTagLookup(traits.root_node_tag)) {
      LOG(ERROR) << GetRootNodeDoesNotExistError(traits.root_node_tag);
      return false;
    }
    sync_api::WriteNode create_node(trans);
    if (!create_node.InitUniqueByCreation(traits.model_type, root, id)) {
      LOG(ERROR) << "Could not create node for extension " << id;
      return false;
    }
    (*traits.extension_specifics_setter)(specifics, &create_node);
  }
  return true;
}

}  // namespace

bool FlushExtensionData(const ExtensionSyncTraits& traits,
                        const ExtensionDataMap& extension_data_map,
                        ExtensionServiceInterface* extension_service,
                        sync_api::UserShare* user_share) {
  sync_api::WriteTransaction trans(FROM_HERE, user_share);
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(traits.root_node_tag)) {
    LOG(ERROR) << GetRootNodeDoesNotExistError(traits.root_node_tag);
    return false;
  }

  // Update server and client as necessary.
  for (ExtensionDataMap::const_iterator it = extension_data_map.begin();
       it != extension_data_map.end(); ++it) {
    const ExtensionSyncData& extension_data = it->second;
    if (!UpdateServer(traits, extension_data, &trans)) {
      LOG(ERROR) << "Could not update server data for extension "
                 << it->first;
      return false;
    }
    extension_service->ProcessSyncData(extension_data,
                                       traits.is_valid_and_syncable);
  }
  return true;
}

bool UpdateServerData(const ExtensionSyncTraits& traits,
                      const Extension& extension,
                      const ExtensionServiceInterface& extension_service,
                      sync_api::UserShare* user_share,
                      std::string* error) {
  const std::string& id = extension.id();
  ExtensionSyncData data;
  if (!extension_service.GetSyncData(
          extension, traits.is_valid_and_syncable, &data)) {
    *error =
        std::string("UpdateServerData() called for invalid or "
                    "unsyncable extension ") + id;
    LOG(DFATAL) << *error;
    return false;
  }

  sync_api::WriteTransaction trans(FROM_HERE, user_share);
  if (!UpdateServer(traits, data, &trans)) {
    *error =
        std::string("Could not update server data for extension ") + id;
    LOG(ERROR) << *error;
    return false;
  }
  return true;
}

void RemoveServerData(const ExtensionSyncTraits& traits,
                      const std::string& id,
                      sync_api::UserShare* user_share) {
  sync_api::WriteTransaction trans(FROM_HERE, user_share);
  sync_api::WriteNode write_node(&trans);
  if (write_node.InitByClientTagLookup(traits.model_type, id)) {
    write_node.Remove();
  } else {
    LOG(ERROR) << "Server data does not exist for extension " << id;
  }
}

}  // namespace browser_sync
