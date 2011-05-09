// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_SERVICE_H_
#pragma once

#include "chrome/browser/sync/engine/syncapi.h"  // TODO(zea): remove this.
#include "chrome/browser/sync/glue/model_associator.h"

class ProfileSyncService;

namespace browser_sync {
class GenericChangeProcessor;
}

// TODO(sync): Define SyncData type for passing sync changes here.

// TODO(sync): Have this become an independent class and deprecate
// AssociatorInterface.
class SyncableService : public browser_sync::AssociatorInterface {
 public:
  SyncableService();
  virtual ~SyncableService();

  // Future methods for SyncableService:
  // StartSyncing
  // StopSyncing
  // GetAllSyncData
  // ProcessNewSyncData (replaces ApplyChangeFromSync)

  // TODO(sync): remove these once we switch to the real SyncableService
  // interface. This just keeps us going while we use the model associator/
  // change processor way of things.

  // AssociatorInterface implementation.
  virtual bool AssociateModels() = 0;
  virtual bool DisassociateModels() = 0;
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) = 0;
  virtual void AbortAssociation() = 0;
  virtual bool CryptoReadyIfNecessary() = 0;

  // Receive changes from change processor (the ChangeRecord dependency is
  // the reason we must include syncapi.h).
  virtual void ApplyChangesFromSync(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count) = 0;
  virtual void SetupSync(
      ProfileSyncService* sync_service,
      browser_sync::GenericChangeProcessor* change_processor) = 0;
};

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_SERVICE_H_
