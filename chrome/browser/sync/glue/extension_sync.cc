// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_sync.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/extension_data.h"
#include "chrome/browser/sync/glue/extension_sync_traits.h"
#include "chrome/browser/sync/glue/extension_util.h"
#include "chrome/browser/sync/profile_sync_service.h"

namespace browser_sync {

bool RootNodeHasChildren(const char* tag,
                         ProfileSyncService* sync_service,
                         bool* has_children) {
  CHECK(has_children);
  *has_children = false;
  sync_api::ReadTransaction trans(sync_service->GetUserShare());
  sync_api::ReadNode node(&trans);
  if (!node.InitByTagLookup(tag)) {
    LOG(ERROR) << "Root node with tag " << tag << " does not exist";
    return false;
  }
  *has_children = node.GetFirstChildId() != sync_api::kInvalidId;
  return true;
}

ExtensionService* GetExtensionServiceFromProfile(
    Profile* profile) {
  CHECK(profile);
  ExtensionService* extensions_service = profile->GetExtensionService();
  CHECK(extensions_service);
  return extensions_service;
}

namespace {

ExtensionService* GetExtensionServiceFromProfileSyncService(
    ProfileSyncService* sync_service) {
  CHECK(sync_service);
  return GetExtensionServiceFromProfile(sync_service->profile());
}

// Updates the value in |extension_data_map| from the given data,
// creating an entry if necessary.  Returns a pointer to the
// updated/created ExtensionData object.
ExtensionData* SetOrCreateExtensionData(
    ExtensionDataMap* extension_data_map,
    ExtensionData::Source source,
    bool merge_user_properties,
    const sync_pb::ExtensionSpecifics& data) {
  DcheckIsExtensionSpecificsValid(data);
  const std::string& extension_id = data.id();
  std::pair<ExtensionDataMap::iterator, bool> result =
      extension_data_map->insert(
          std::make_pair(extension_id,
                         ExtensionData::FromData(source, data)));
  ExtensionData* extension_data = &result.first->second;
  if (result.second) {
    // The value was just inserted, so it shouldn't need an update
    // from source.
    DCHECK(!extension_data->NeedsUpdate(source));
  } else {
    extension_data->SetData(source, merge_user_properties, data);
  }
  return extension_data;
}

// Reads the client data for each extension in |extensions| to be
// synced and updates |extension_data_map|.  Puts all unsynced
// extensions in |unsynced_extensions|.
void ReadClientDataFromExtensionList(
    const ExtensionList& extensions,
    IsValidAndSyncablePredicate is_valid_and_syncable,
    ExtensionService* extensions_service,
    std::set<std::string>* unsynced_extensions,
    ExtensionDataMap* extension_data_map) {
  for (ExtensionList::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    CHECK(*it);
    const Extension& extension = **it;
    if (is_valid_and_syncable(extension)) {
      sync_pb::ExtensionSpecifics client_specifics;
      GetExtensionSpecifics(extension, extensions_service->extension_prefs(),
                            &client_specifics);
      DcheckIsExtensionSpecificsValid(client_specifics);
      const ExtensionData& extension_data =
          *SetOrCreateExtensionData(
              extension_data_map, ExtensionData::CLIENT,
              true, client_specifics);
      DcheckIsExtensionSpecificsValid(extension_data.merged_data());
      // Assumes this is called before any server data is read.
      DCHECK(extension_data.NeedsUpdate(ExtensionData::SERVER));
      DCHECK(!extension_data.NeedsUpdate(ExtensionData::CLIENT));
    } else {
      unsynced_extensions->insert(extension.id());
    }
  }
}

// Simply calls ReadClientDataFromExtensionList() on the list of
// enabled and disabled extensions from |extensions_service|.
void SlurpClientData(
    IsValidAndSyncablePredicate is_valid_and_syncable,
    ExtensionService* extensions_service,
    std::set<std::string>* unsynced_extensions,
    ExtensionDataMap* extension_data_map) {
  const ExtensionList* extensions = extensions_service->extensions();
  CHECK(extensions);
  ReadClientDataFromExtensionList(
      *extensions, is_valid_and_syncable, extensions_service,
      unsynced_extensions, extension_data_map);

  const ExtensionList* disabled_extensions =
      extensions_service->disabled_extensions();
  CHECK(disabled_extensions);
  ReadClientDataFromExtensionList(
      *disabled_extensions, is_valid_and_syncable, extensions_service,
      unsynced_extensions, extension_data_map);
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
// updates |extension_data_map|.  Skips all extensions in
// |unsynced_extensions|.
bool SlurpServerData(
    const char* root_node_tag,
    const ExtensionSpecificsGetter extension_specifics_getter,
    const std::set<std::string>& unsynced_extensions,
    ProfileSyncService* sync_service,
    ExtensionDataMap* extension_data_map) {
  sync_api::WriteTransaction trans(sync_service->GetUserShare());
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
    if (!IsExtensionSpecificsValid(server_data)) {
      LOG(ERROR) << "Invalid extensions specifics for id " << id;
      return false;
    }
    // Don't process server data for extensions we know are
    // unsyncable.  This doesn't catch everything, as if we don't
    // have the extension already installed we can't check, but we
    // also check at extension install time.
    if (unsynced_extensions.find(server_data.id()) ==
        unsynced_extensions.end()) {
      // Pass in false for merge_user_properties so client user
      // settings always take precedence.
      const ExtensionData& extension_data =
          *SetOrCreateExtensionData(
              extension_data_map, ExtensionData::SERVER, false, server_data);
      DcheckIsExtensionSpecificsValid(extension_data.merged_data());
    }
    id = sync_node.GetSuccessorId();
  }
  return true;
}

}  // namespace

bool SlurpExtensionData(const ExtensionSyncTraits& traits,
                        ProfileSyncService* sync_service,
                        ExtensionDataMap* extension_data_map) {
  ExtensionService* extensions_service =
      GetExtensionServiceFromProfileSyncService(sync_service);
  std::set<std::string> unsynced_extensions;

  // Read client-side data first so server data takes precedence, and
  // also so we have an idea of which extensions are unsyncable.
  SlurpClientData(
      traits.is_valid_and_syncable, extensions_service,
      &unsynced_extensions, extension_data_map);

  if (!SlurpServerData(
          traits.root_node_tag, traits.extension_specifics_getter,
          unsynced_extensions, sync_service, extension_data_map)) {
    return false;
  }
  return true;
}

namespace {

// Updates the server data from the given extension data.
// extension_data->ServerNeedsUpdate() must hold before this function
// is called.  Returns whether or not the update was successful.  If
// the update was successful, extension_data->ServerNeedsUpdate() will
// be false after this function is called.  This function leaves
// extension_data->ClientNeedsUpdate() unchanged.
bool UpdateServer(
    const ExtensionSyncTraits& traits,
    ExtensionData* extension_data,
    sync_api::WriteTransaction* trans) {
  DCHECK(extension_data->NeedsUpdate(ExtensionData::SERVER));
  const sync_pb::ExtensionSpecifics& specifics =
      extension_data->merged_data();
  const std::string& id = specifics.id();
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
  bool old_client_needs_update =
      extension_data->NeedsUpdate(ExtensionData::CLIENT);
  extension_data->ResolveData(ExtensionData::SERVER);
  DCHECK(!extension_data->NeedsUpdate(ExtensionData::SERVER));
  DCHECK_EQ(extension_data->NeedsUpdate(ExtensionData::CLIENT),
            old_client_needs_update);
  return true;
}

// Tries to update the client data from the given extension data.
// extension_data->ServerNeedsUpdate() must not hold and
// extension_data->ClientNeedsUpdate() must hold before this function
// is called.  If the update was successful,
// extension_data->ClientNeedsUpdate() will be false after this
// function is called.  Otherwise, the extension needs updating to a
// new version.
void TryUpdateClient(
    IsValidAndSyncablePredicate is_valid_and_syncable,
    ExtensionService* extensions_service,
    ExtensionData* extension_data) {
  DCHECK(!extension_data->NeedsUpdate(ExtensionData::SERVER));
  DCHECK(extension_data->NeedsUpdate(ExtensionData::CLIENT));
  const sync_pb::ExtensionSpecifics& specifics =
      extension_data->merged_data();
  DcheckIsExtensionSpecificsValid(specifics);
  const std::string& id = specifics.id();
  const Extension* extension = extensions_service->GetExtensionById(id, true);
  if (extension) {
    if (!is_valid_and_syncable(*extension)) {
      LOG(DFATAL) << "TryUpdateClient() called for non-syncable extension "
                  << extension->id();
      return;
    }
    SetExtensionProperties(specifics, extensions_service, extension);
    {
      sync_pb::ExtensionSpecifics extension_specifics;
      GetExtensionSpecifics(*extension, extensions_service->extension_prefs(),
                            &extension_specifics);
      DCHECK(AreExtensionSpecificsUserPropertiesEqual(
          specifics, extension_specifics))
          << ExtensionSpecificsToString(specifics) << ", "
          << ExtensionSpecificsToString(extension_specifics);
    }
    if (!IsExtensionOutdated(*extension, specifics)) {
      extension_data->ResolveData(ExtensionData::CLIENT);
      DCHECK(!extension_data->NeedsUpdate(ExtensionData::CLIENT));
    }
  } else {
    GURL update_url(specifics.update_url());
    // TODO(akalin): Replace silent update with a list of enabled
    // permissions.
    extensions_service->AddPendingExtensionFromSync(
        id, update_url,
        is_valid_and_syncable,
        true,  // install_silently
        specifics.enabled(),
        specifics.incognito_enabled());
  }
  DCHECK(!extension_data->NeedsUpdate(ExtensionData::SERVER));
}

// Kick off a run of the extension updater.
//
// TODO(akalin): Combine this with the similar function in
// theme_util.cc.
void NudgeExtensionUpdater(ExtensionService* extensions_service) {
  ExtensionUpdater* extension_updater = extensions_service->updater();
  // Auto-updates should now be on always (see the construction of the
  // ExtensionService in ProfileImpl::InitExtensions()).
  if (extension_updater) {
    extension_updater->CheckNow();
  } else {
    LOG(DFATAL) << "Extension updater unexpectedly NULL; "
                << "auto-updates may be turned off";
  }
}

}  // namespace

bool FlushExtensionData(const ExtensionSyncTraits& traits,
                        const ExtensionDataMap& extension_data_map,
                        ProfileSyncService* sync_service) {
  sync_api::WriteTransaction trans(sync_service->GetUserShare());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(traits.root_node_tag)) {
    LOG(ERROR) << GetRootNodeDoesNotExistError(traits.root_node_tag);
    return false;
  }

  ExtensionService* extensions_service =
      GetExtensionServiceFromProfileSyncService(sync_service);

  // Update server and client as necessary.
  bool should_nudge_extension_updater = false;
  for (ExtensionDataMap::const_iterator it = extension_data_map.begin();
       it != extension_data_map.end(); ++it) {
    ExtensionData extension_data = it->second;
    // Update server first.
    if (extension_data.NeedsUpdate(ExtensionData::SERVER)) {
      if (!UpdateServer(traits, &extension_data, &trans)) {
        LOG(ERROR) << "Could not update server data for extension "
                   << it->first;
        return false;
      }
    }
    DCHECK(!extension_data.NeedsUpdate(ExtensionData::SERVER));
    if (extension_data.NeedsUpdate(ExtensionData::CLIENT)) {
      TryUpdateClient(traits.is_valid_and_syncable,
                      extensions_service, &extension_data);
      if (extension_data.NeedsUpdate(ExtensionData::CLIENT)) {
        should_nudge_extension_updater = true;
      }
    }
    DCHECK(!extension_data.NeedsUpdate(ExtensionData::SERVER));
  }

  if (should_nudge_extension_updater) {
    NudgeExtensionUpdater(extensions_service);
  }

  return true;
}

bool UpdateServerData(const ExtensionSyncTraits& traits,
                      const Extension& extension,
                      ProfileSyncService* sync_service,
                      std::string* error) {
  const std::string& id = extension.id();
  if (!traits.is_valid_and_syncable(extension)) {
    *error =
        std::string("UpdateServerData() called for invalid or "
                    "unsyncable extension ") + id;
    LOG(DFATAL) << *error;
    return false;
  }

  ExtensionService* extensions_service =
      GetExtensionServiceFromProfileSyncService(sync_service);
  sync_pb::ExtensionSpecifics client_data;
  GetExtensionSpecifics(extension, extensions_service->extension_prefs(),
                        &client_data);
  DcheckIsExtensionSpecificsValid(client_data);
  ExtensionData extension_data =
      ExtensionData::FromData(ExtensionData::CLIENT, client_data);

  sync_api::WriteTransaction trans(sync_service->GetUserShare());

  sync_api::ReadNode node(&trans);
  if (node.InitByClientTagLookup(traits.model_type, id)) {
    sync_pb::ExtensionSpecifics server_data =
        (*traits.extension_specifics_getter)(node);
    if (IsExtensionSpecificsValid(server_data)) {
      // If server node exists and is valid, update |extension_data|
      // from it (but with it taking precedence).
      extension_data =
          ExtensionData::FromData(ExtensionData::SERVER, server_data);
      extension_data.SetData(ExtensionData::CLIENT, true, client_data);
    } else {
      LOG(ERROR) << "Invalid extensions specifics for id " << id
                 << "; treating as empty";
    }
  }

  if (extension_data.NeedsUpdate(ExtensionData::SERVER)) {
    if (!UpdateServer(traits, &extension_data, &trans)) {
      *error =
          std::string("Could not update server data for extension ") + id;
      LOG(ERROR) << *error;
      return false;
    }
  }
  DCHECK(!extension_data.NeedsUpdate(ExtensionData::SERVER));
  // Client may still need updating, e.g. if we disable an extension
  // while it's being auto-updated.  If so, then we'll be called
  // again once the auto-update is finished.
  //
  // TODO(akalin): Figure out a way to tell when the above happens,
  // so we know exactly what NeedsUpdate(CLIENT) should return.
  return true;
}

void RemoveServerData(const ExtensionSyncTraits& traits,
                      const std::string& id,
                      ProfileSyncService* sync_service) {
  sync_api::WriteTransaction trans(sync_service->GetUserShare());
  sync_api::WriteNode write_node(&trans);
  if (write_node.InitByClientTagLookup(traits.model_type, id)) {
    write_node.Remove();
  } else {
    LOG(ERROR) << "Server data does not exist for extension " << id;
  }
}

void UpdateClient(const ExtensionSyncTraits& traits,
                  const sync_pb::ExtensionSpecifics& server_data,
                  ExtensionService* extensions_service) {
  DcheckIsExtensionSpecificsValid(server_data);
  ExtensionData extension_data =
      ExtensionData::FromData(ExtensionData::SERVER, server_data);
  const Extension* extension =
      extensions_service->GetExtensionById(server_data.id(), true);
  if (extension) {
    if (!traits.is_valid_and_syncable(*extension)) {
      LOG(WARNING) << "Ignoring server data for invalid or "
                   << "non-syncable extension " << extension->id();
      return;
    }
    sync_pb::ExtensionSpecifics client_data;
    GetExtensionSpecifics(*extension, extensions_service->extension_prefs(),
                          &client_data);
    DcheckIsExtensionSpecificsValid(client_data);
    extension_data =
        ExtensionData::FromData(ExtensionData::CLIENT, client_data);
    extension_data.SetData(ExtensionData::SERVER, true, server_data);
  }
  DCHECK(!extension_data.NeedsUpdate(ExtensionData::SERVER));
  if (extension_data.NeedsUpdate(ExtensionData::CLIENT)) {
    TryUpdateClient(traits.is_valid_and_syncable,
                    extensions_service, &extension_data);
    if (extension_data.NeedsUpdate(ExtensionData::CLIENT)) {
      NudgeExtensionUpdater(extensions_service);
    }
  }
  DCHECK(!extension_data.NeedsUpdate(ExtensionData::SERVER));
}

void RemoveFromClient(const ExtensionSyncTraits& traits,
                      const std::string& id,
                      ExtensionService* extensions_service) {
  const Extension* extension = extensions_service->GetExtensionById(id, true);
  if (extension) {
    if (traits.is_valid_and_syncable(*extension)) {
      extensions_service->UninstallExtension(id, false);
    } else {
      LOG(WARNING) << "Ignoring server data for invalid or "
                   << "non-syncable extension " << extension->id();
    }
  } else {
    LOG(ERROR) << "Trying to uninstall nonexistent extension " << id;
  }
}

}  // namespace browser_sync
