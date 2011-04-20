// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_model_associator.h"

#include "base/logging.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/extension_data.h"
#include "chrome/browser/sync/glue/extension_sync_traits.h"
#include "chrome/browser/sync/glue/extension_sync.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/browser/sync/syncable/nigori_util.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

ExtensionModelAssociator::ExtensionModelAssociator(
    const ExtensionSyncTraits& traits,
    ExtensionServiceInterface* extension_service,
    sync_api::UserShare* user_share)
    : traits_(traits), extension_service_(extension_service),
      user_share_(user_share) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension_service_);
  DCHECK(user_share_);
}

ExtensionModelAssociator::~ExtensionModelAssociator() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool ExtensionModelAssociator::AssociateModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ExtensionDataMap extension_data_map;
  if (!SlurpExtensionData(
          traits_, *extension_service_, user_share_, &extension_data_map)) {
    return false;
  }
  if (!FlushExtensionData(
          traits_, extension_data_map, extension_service_, user_share_)) {
    return false;
  }

  return true;
}

bool ExtensionModelAssociator::DisassociateModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Nothing to do.
  return true;
}

bool ExtensionModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return RootNodeHasChildren(traits_.root_node_tag, user_share_, has_nodes);
}

bool ExtensionModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(user_share_);
  const syncable::ModelTypeSet& encrypted_types =
      GetEncryptedDataTypes(trans.GetWrappedTrans());
  return encrypted_types.count(traits_.model_type) == 0 ||
      trans.GetCryptographer()->is_ready();
}

}  // namespace browser_sync
