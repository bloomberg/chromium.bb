// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_model_associator.h"

#include "base/logging.h"
#include "chrome/browser/sync/glue/extension_data.h"
#include "chrome/browser/sync/glue/extension_sync_traits.h"
#include "chrome/browser/sync/glue/extension_sync.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

ExtensionModelAssociator::ExtensionModelAssociator(
    const ExtensionSyncTraits& traits, ProfileSyncService* sync_service)
    : traits_(traits), sync_service_(sync_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(sync_service_);
}

ExtensionModelAssociator::~ExtensionModelAssociator() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool ExtensionModelAssociator::AssociateModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ExtensionDataMap extension_data_map;
  if (!SlurpExtensionData(traits_, sync_service_, &extension_data_map)) {
    return false;
  }
  if (!FlushExtensionData(traits_, extension_data_map, sync_service_)) {
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
  return RootNodeHasChildren(traits_.root_node_tag, sync_service_, has_nodes);
}

}  // namespace browser_sync
