// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/api/syncable_service.h"
#include "sync/internal_api/public/base/model_type.h"

namespace browser_sync {

// This factory provides sync driver code with the model type specific sync/api
// service (like SyncableService) implementations.
class SyncApiComponentFactory {
 public:
  // Returns a weak pointer to the syncable service specified by |type|.
  // Weak pointer may be unset if service is already destroyed.
  // Note: Should only be called from the model type thread.
  virtual base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) = 0;

  // Returns the custom attachment store for a model type, if there is one.
  // May return NULL, which implies sync should use a default implementation.
  // Note: Should only be called from the model type thread.
  virtual scoped_ptr<syncer::AttachmentStore>
      CreateCustomAttachmentStoreForType(syncer::ModelType type) = 0;
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_H_
