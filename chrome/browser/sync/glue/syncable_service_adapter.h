// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCABLE_SERVICE_ADAPTER_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCABLE_SERVICE_ADAPTER_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/glue/model_associator.h"

class SyncableService;

namespace browser_sync {

class GenericChangeProcessor;

// An adapter to handle transitioning from model associator's to syncable
// services. Implements the model associator interface, invoking the
// provided SyncableService as necessary.
class SyncableServiceAdapter : public AssociatorInterface {
 public:
  explicit SyncableServiceAdapter(syncable::ModelType type,
                                  SyncableService* service,
                                  GenericChangeProcessor* sync_processor);
  virtual ~SyncableServiceAdapter();

  // AssociatorInterface implementation.
  virtual bool AssociateModels() OVERRIDE;
  virtual bool DisassociateModels() OVERRIDE;
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) OVERRIDE;
  virtual void AbortAssociation() OVERRIDE;
  virtual bool CryptoReadyIfNecessary() OVERRIDE;

 private:
  // Whether the SyncableService we wrap is currently syncing or not.
  bool syncing_;

  // The data type being provided by this service.
  syncable::ModelType type_;

  // A pointer to the actual local syncable service, which performs all the
  // real work.
  SyncableService* service_;

  // The datatype's SyncChange handler that interacts with the sync db.
  GenericChangeProcessor* sync_processor_;

  DISALLOW_COPY_AND_ASSIGN(SyncableServiceAdapter);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCABLE_SERVICE_ADAPTER_H_
