// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_model_associator.h"

#include <map>
#include <utility>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/extension_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/common/extensions/extension.h"

namespace browser_sync {

namespace {

static const char kExtensionsTag[] = "google_chrome_extensions";

static const char kNoExtensionsFolderError[] =
    "Server did not create the top-level extensions node. We "
    "might be running against an out-of-date server.";

typedef std::map<std::string, ExtensionData> ExtensionDataMap;

ExtensionData* SetOrCreateData(
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

void GetSyncableExtensionsClientData(
    const ExtensionList& extensions,
    ExtensionsService* extensions_service,
    std::set<std::string>* unsyncable_extensions,
    ExtensionDataMap* extension_data_map) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  for (ExtensionList::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    CHECK(*it);
    const Extension& extension = **it;
    if (IsExtensionSyncable(extension)) {
      sync_pb::ExtensionSpecifics client_specifics;
      GetExtensionSpecifics(extension, extensions_service,
                            &client_specifics);
      DcheckIsExtensionSpecificsValid(client_specifics);
      const ExtensionData& extension_data =
          *SetOrCreateData(extension_data_map,
                           ExtensionData::CLIENT, true, client_specifics);
      DcheckIsExtensionSpecificsValid(extension_data.merged_data());
      // Assumes this is called before any server data is read.
      DCHECK(extension_data.NeedsUpdate(ExtensionData::SERVER));
      DCHECK(!extension_data.NeedsUpdate(ExtensionData::CLIENT));
    } else {
      unsyncable_extensions->insert(extension.id());
    }
  }
}

}  // namespace

ExtensionModelAssociator::ExtensionModelAssociator(
    ProfileSyncService* sync_service) : sync_service_(sync_service) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(sync_service_);
}

ExtensionModelAssociator::~ExtensionModelAssociator() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

bool ExtensionModelAssociator::AssociateModels() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  sync_api::WriteTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kExtensionsTag)) {
    LOG(ERROR) << kNoExtensionsFolderError;
    return false;
  }

  std::set<std::string> unsyncable_extensions;
  ExtensionDataMap extension_data_map;

  // Read client-side data.  Do this first so server data takes
  // precedence.
  {
    ExtensionsService* extensions_service = GetExtensionsService();

    const ExtensionList* extensions = extensions_service->extensions();
    CHECK(extensions);
    GetSyncableExtensionsClientData(
        *extensions, extensions_service,
        &unsyncable_extensions, &extension_data_map);

    const ExtensionList* disabled_extensions =
        extensions_service->disabled_extensions();
    CHECK(disabled_extensions);
    GetSyncableExtensionsClientData(
        *disabled_extensions, extensions_service,
        &unsyncable_extensions, &extension_data_map);
  }

  // Read server-side data.
  {
    int64 id = root.GetFirstChildId();
    while (id != sync_api::kInvalidId) {
      sync_api::ReadNode sync_node(&trans);
      if (!sync_node.InitByIdLookup(id)) {
        LOG(ERROR) << "Failed to fetch sync node for id " << id;
        return false;
      }
      const sync_pb::ExtensionSpecifics& server_data =
          sync_node.GetExtensionSpecifics();
      if (!IsExtensionSpecificsValid(server_data)) {
        LOG(ERROR) << "Invalid extensions specifics for id " << id;
        return false;
      }
      // Don't process server data for extensions we know are
      // unsyncable.  This doesn't catch everything, as if we don't
      // have the extension already installed we can't check, but we
      // also check at extension install time.
      if (unsyncable_extensions.find(server_data.id()) ==
          unsyncable_extensions.end()) {
        // Pass in false for merge_user_properties so client user
        // settings always take precedence.
        const ExtensionData& extension_data =
            *SetOrCreateData(&extension_data_map,
                             ExtensionData::SERVER, false, server_data);
        DcheckIsExtensionSpecificsValid(extension_data.merged_data());
      }
      id = sync_node.GetSuccessorId();
    }
  }

  // Update server and client as necessary.
  bool should_nudge_extension_updater = false;
  for (ExtensionDataMap::iterator it = extension_data_map.begin();
       it != extension_data_map.end(); ++it) {
    ExtensionData* extension_data = &it->second;
    // Update server first.
    if (extension_data->NeedsUpdate(ExtensionData::SERVER)) {
      if (!UpdateServer(extension_data, &trans, root)) {
        LOG(ERROR) << "Could not update server data for extension "
                   << it->first;
        return false;
      }
    }
    DCHECK(!extension_data->NeedsUpdate(ExtensionData::SERVER));
    if (extension_data->NeedsUpdate(ExtensionData::CLIENT)) {
      TryUpdateClient(extension_data);
      if (extension_data->NeedsUpdate(ExtensionData::CLIENT)) {
        should_nudge_extension_updater = true;
      }
    }
    DCHECK(!extension_data->NeedsUpdate(ExtensionData::SERVER));
  }

  if (should_nudge_extension_updater) {
    NudgeExtensionUpdater();
  }

  return true;
}

bool ExtensionModelAssociator::DisassociateModels() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // Nothing to do.
  return true;
}

bool ExtensionModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  CHECK(has_nodes);
  *has_nodes = false;
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kExtensionsTag)) {
    LOG(ERROR) << kNoExtensionsFolderError;
    return false;
  }
  // The sync model has user created nodes iff the extensions folder has
  // any children.
  *has_nodes = root.GetFirstChildId() != sync_api::kInvalidId;
  return true;
}

bool ExtensionModelAssociator::OnClientUpdate(const std::string& id) {
  sync_api::WriteTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(kExtensionsTag)) {
    LOG(ERROR) << kNoExtensionsFolderError;
    return false;
  }
  ExtensionsService* extensions_service = GetExtensionsService();
  Extension* extension = extensions_service->GetExtensionById(id, true);
  if (extension) {
    if (!IsExtensionSyncable(*extension)) {
      LOG(DFATAL) << "OnClientUpdate() called for non-syncable extension "
                  << id;
      return false;
    }
    sync_pb::ExtensionSpecifics client_data;
    GetExtensionSpecifics(*extension, extensions_service, &client_data);
    DcheckIsExtensionSpecificsValid(client_data);
    ExtensionData extension_data =
        ExtensionData::FromData(ExtensionData::CLIENT, client_data);
    sync_pb::ExtensionSpecifics server_data;
    if (GetExtensionDataFromServer(id, &trans, root, &server_data)) {
      extension_data =
          ExtensionData::FromData(ExtensionData::SERVER, server_data);
      extension_data.SetData(ExtensionData::CLIENT, true, client_data);
    }
    if (extension_data.NeedsUpdate(ExtensionData::SERVER)) {
      if (!UpdateServer(&extension_data, &trans, root)) {
        LOG(ERROR) << "Could not update server data for extension " << id;
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
  } else {
    sync_api::WriteNode write_node(&trans);
    if (write_node.InitByClientTagLookup(syncable::EXTENSIONS, id)) {
      write_node.Remove();
    } else {
      LOG(ERROR) << "Trying to remove server data for "
                 << "nonexistent extension " << id;
    }
  }
  return true;
}

void ExtensionModelAssociator::OnServerUpdate(
    const sync_pb::ExtensionSpecifics& server_data) {
  DcheckIsExtensionSpecificsValid(server_data);
  ExtensionData extension_data =
      ExtensionData::FromData(ExtensionData::SERVER, server_data);
  ExtensionsService* extensions_service = GetExtensionsService();
  Extension* extension =
      extensions_service->GetExtensionById(server_data.id(), true);
  if (extension) {
    if (!IsExtensionSyncable(*extension)) {
      // Ignore updates for non-syncable extensions (we may get those
      // for extensions that were previously syncable).
      return;
    }
    sync_pb::ExtensionSpecifics client_data;
    GetExtensionSpecifics(*extension, extensions_service, &client_data);
    DcheckIsExtensionSpecificsValid(client_data);
    extension_data =
        ExtensionData::FromData(ExtensionData::CLIENT, client_data);
    extension_data.SetData(ExtensionData::SERVER, true, server_data);
  }
  DCHECK(!extension_data.NeedsUpdate(ExtensionData::SERVER));
  if (extension_data.NeedsUpdate(ExtensionData::CLIENT)) {
    TryUpdateClient(&extension_data);
    if (extension_data.NeedsUpdate(ExtensionData::CLIENT)) {
      NudgeExtensionUpdater();
    }
  }
  DCHECK(!extension_data.NeedsUpdate(ExtensionData::SERVER));
}

void ExtensionModelAssociator::OnServerRemove(const std::string& id) {
  ExtensionsService* extensions_service = GetExtensionsService();
  Extension* extension = extensions_service->GetExtensionById(id, true);
  if (extension) {
    if (IsExtensionSyncable(*extension)) {
      extensions_service->UninstallExtension(id, false);
    }
  } else {
    LOG(ERROR) << "Trying to uninstall nonexistent extension " << id;
  }
}

ExtensionsService* ExtensionModelAssociator::GetExtensionsService() {
  CHECK(sync_service_);
  Profile* profile = sync_service_->profile();
  CHECK(profile);
  ExtensionsService* extensions_service = profile->GetExtensionsService();
  CHECK(extensions_service);
  return extensions_service;
}

bool ExtensionModelAssociator::GetExtensionDataFromServer(
    const std::string& id, sync_api::WriteTransaction* trans,
    const sync_api::ReadNode& root,
    sync_pb::ExtensionSpecifics* server_data) {
  sync_api::ReadNode sync_node(trans);
  if (!sync_node.InitByClientTagLookup(syncable::EXTENSIONS, id)) {
    LOG(ERROR) << "Failed to fetch sync node for id " << id;
    return false;
  }
  const sync_pb::ExtensionSpecifics& read_server_data =
      sync_node.GetExtensionSpecifics();
  if (!IsExtensionSpecificsValid(read_server_data)) {
    LOG(ERROR) << "Invalid extensions specifics for id " << id;
    return false;
  }
  *server_data = read_server_data;
  return true;
}

namespace {

void SetNodeData(const sync_pb::ExtensionSpecifics& specifics,
                 sync_api::WriteNode* node) {
  node->SetTitle(UTF8ToWide(specifics.name()));
  node->SetExtensionSpecifics(specifics);
}

}  // namespace

bool ExtensionModelAssociator::UpdateServer(
    ExtensionData* extension_data,
    sync_api::WriteTransaction* trans,
    const sync_api::ReadNode& root) {
  DCHECK(extension_data->NeedsUpdate(ExtensionData::SERVER));
  const sync_pb::ExtensionSpecifics& specifics =
      extension_data->merged_data();
  const std::string& id = specifics.id();
  sync_api::WriteNode write_node(trans);
  if (write_node.InitByClientTagLookup(syncable::EXTENSIONS, id)) {
    SetNodeData(specifics, &write_node);
  } else {
    sync_api::WriteNode create_node(trans);
    if (!create_node.InitUniqueByCreation(syncable::EXTENSIONS, root, id)) {
      LOG(ERROR) << "Could not create node for extension " << id;
      return false;
    }
    SetNodeData(specifics, &create_node);
  }
  bool old_client_needs_update =
      extension_data->NeedsUpdate(ExtensionData::CLIENT);
  extension_data->ResolveData(ExtensionData::SERVER);
  DCHECK(!extension_data->NeedsUpdate(ExtensionData::SERVER));
  DCHECK_EQ(extension_data->NeedsUpdate(ExtensionData::CLIENT),
            old_client_needs_update);
  return true;
}

void ExtensionModelAssociator::TryUpdateClient(
    ExtensionData* extension_data) {
  DCHECK(!extension_data->NeedsUpdate(ExtensionData::SERVER));
  DCHECK(extension_data->NeedsUpdate(ExtensionData::CLIENT));
  const sync_pb::ExtensionSpecifics& specifics =
      extension_data->merged_data();
  DcheckIsExtensionSpecificsValid(specifics);
  ExtensionsService* extensions_service = GetExtensionsService();
  const std::string& id = specifics.id();
  Extension* extension = extensions_service->GetExtensionById(id, true);
  if (extension) {
    if (!IsExtensionSyncable(*extension)) {
      LOG(DFATAL) << "TryUpdateClient() called for non-syncable extension "
                  << extension->id();
      return;
    }
    SetExtensionProperties(specifics, extensions_service, extension);
    {
      sync_pb::ExtensionSpecifics extension_specifics;
      GetExtensionSpecifics(*extension, extensions_service,
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
    extensions_service->AddPendingExtension(
        id, update_url, false, true,
        specifics.enabled(), specifics.incognito_enabled());
  }
  DCHECK(!extension_data->NeedsUpdate(ExtensionData::SERVER));
}

void ExtensionModelAssociator::NudgeExtensionUpdater() {
  ExtensionUpdater* extension_updater = GetExtensionsService()->updater();
  // Auto-updates should now be on always (see the construction of the
  // ExtensionsService in ProfileImpl::InitExtensions()).
  if (extension_updater) {
    extension_updater->CheckNow();
  } else {
    LOG(DFATAL) << "Extension updater unexpectedly NULL; "
                << "auto-updates may be turned off";
  }
}

}  // namespace browser_sync
