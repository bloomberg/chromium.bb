// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/app_model_associator.h"

#include "base/logging.h"
#include "base/tracked.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/engine/nigori_util.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/extension_sync_traits.h"
#include "chrome/browser/sync/glue/extension_sync.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

AppModelAssociator::AppModelAssociator(
    ExtensionServiceInterface* extension_service,
    sync_api::UserShare* user_share)
    : traits_(GetAppSyncTraits()), extension_service_(extension_service),
      user_share_(user_share) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension_service_);
  DCHECK(user_share_);
}

AppModelAssociator::~AppModelAssociator() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool AppModelAssociator::AssociateModels(SyncError* error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ExtensionDataMap extension_data_map;
  if (!SlurpExtensionData(
          traits_, *extension_service_, user_share_, &extension_data_map)) {
    error->Reset(FROM_HERE, "Failed to get app data.", model_type());
    return false;
  }
  if (!FlushExtensionData(
          traits_, extension_data_map, extension_service_, user_share_)) {
    error->Reset(FROM_HERE, "Failed to flush app data.", model_type());
    return false;
  }

  return true;
}

bool AppModelAssociator::DisassociateModels(SyncError* error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Nothing to do.
  return true;
}

bool AppModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return RootNodeHasChildren(traits_.root_node_tag, user_share_, has_nodes);
}

void AppModelAssociator::AbortAssociation() {
  // No implementation needed, this associator runs on the main
  // thread.
}

bool AppModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(FROM_HERE, user_share_);
  const syncable::ModelTypeSet& encrypted_types =
      sync_api::GetEncryptedTypes(&trans);
  return encrypted_types.count(traits_.model_type) == 0 ||
      trans.GetCryptographer()->is_ready();
}

}  // namespace browser_sync
