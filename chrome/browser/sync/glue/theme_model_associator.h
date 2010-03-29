// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_THEME_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_THEME_MODEL_ASSOCIATOR_H_

#include "base/basictypes.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/syncable/model_type.h"

class ProfileSyncService;

namespace browser_sync {

class UnrecoverableErrorHandler;

// Contains all logic for associating the Chrome themes model and the
// sync themes model.
class ThemeModelAssociator : public AssociatorInterface {
 public:
  ThemeModelAssociator(ProfileSyncService* sync_service,
                       UnrecoverableErrorHandler* error_handler);
  virtual ~ThemeModelAssociator();

  // Used by profile_sync_test_util.h.
  static syncable::ModelType model_type() { return syncable::THEMES; }

  // AssociatorInterface implementation.
  virtual bool AssociateModels();
  virtual bool DisassociateModels();
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);
  virtual bool ChromeModelHasUserCreatedNodes(bool* has_nodes);

 private:
  ProfileSyncService* sync_service_;
  UnrecoverableErrorHandler* error_handler_;

  DISALLOW_COPY_AND_ASSIGN(ThemeModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_THEME_MODEL_ASSOCIATOR_H_
