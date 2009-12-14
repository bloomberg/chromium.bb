// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_H_

#include "chrome/browser/sync/engine/syncapi.h"

namespace browser_sync {

// Contains all model association related logic to associate the chrome model
// with the sync model.
class ModelAssociator {
 public:
  virtual ~ModelAssociator() {}

  // Iterates through both the sync and the chrome model looking for matched
  // pairs of items. After successful completion, the models should be identical
  // and corresponding. Returns true on success. On failure of this step, we
  // should abort the sync operation and report an error to the user.
  virtual bool AssociateModels() = 0;

  // Clears all the associations between the chrome and sync models.
  virtual bool DisassociateModels() = 0;

  // Returns whether the sync model has nodes other than the permanent tagged
  // nodes.
  virtual bool SyncModelHasUserCreatedNodes() = 0;

  // Returns whether the chrome model has user-created nodes or not.
  virtual bool ChromeModelHasUserCreatedNodes() = 0;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_H_
