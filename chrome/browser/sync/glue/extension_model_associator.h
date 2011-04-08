// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_MODEL_ASSOCIATOR_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/sync/glue/extension_sync_traits.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/syncable/model_type.h"

class ExtensionServiceInterface;

namespace sync_api {
struct UserShare;
}  // namespace sync_api

namespace browser_sync {

// Contains all logic for associating the Chrome extensions model and
// the sync extensions model.
class ExtensionModelAssociator : public AssociatorInterface {
 public:
  // Does not take ownership of sync_service.
  ExtensionModelAssociator(const ExtensionSyncTraits& traits,
                           ExtensionServiceInterface* extension_service,
                           sync_api::UserShare* user_share);
  virtual ~ExtensionModelAssociator();

  // Used by profile_sync_test_util.h.
  static syncable::ModelType model_type() { return syncable::EXTENSIONS; }

  // AssociatorInterface implementation.
  virtual bool AssociateModels();
  virtual bool DisassociateModels();
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);
  virtual void AbortAssociation() {
    // No implementation needed, this associator runs on the main
    // thread.
  }
  virtual bool CryptoReadyIfNecessary();
 private:
  const ExtensionSyncTraits traits_;
  ExtensionServiceInterface* const extension_service_;
  sync_api::UserShare* const user_share_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_MODEL_ASSOCIATOR_H_
