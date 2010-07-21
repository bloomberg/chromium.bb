// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_MODEL_ASSOCIATOR_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/sync/glue/extension_data.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/common/extensions/extension.h"

class ExtensionsService;
class Profile;
class ProfileSyncService;

namespace sync_api {
class ReadNode;
class WriteTransaction;
}  // namespace sync_api

namespace sync_pb {
class ExtensionSpecifics;
}  // namespace sync_pb

namespace browser_sync {

// Contains all logic for associating the Chrome extensions model and
// the sync extensions model.
class ExtensionModelAssociator : public AssociatorInterface {
 public:
  // Does not take ownership of sync_service.
  explicit ExtensionModelAssociator(ProfileSyncService* sync_service);
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

  // Used by ExtensionChangeProcessor.
  //
  // TODO(akalin): These functions can actually be moved to the
  // ChangeProcessor after some refactoring.

  // TODO(akalin): Return an error string instead of just a bool.
  bool OnClientUpdate(const std::string& id);
  void OnServerUpdate(const sync_pb::ExtensionSpecifics& server_data);
  void OnServerRemove(const std::string& id);

 private:
  // Returns the extension service from |sync_service_|.  Never
  // returns NULL.
  ExtensionsService* GetExtensionsService();

  bool GetExtensionDataFromServer(
      const std::string& id, sync_api::WriteTransaction* trans,
      const sync_api::ReadNode& root,
      sync_pb::ExtensionSpecifics* server_data);

  // Updates the server data from the given extension data.
  // extension_data->ServerNeedsUpdate() must hold before this
  // function is called.  Returns whether or not the update was
  // successful.  If the update was successful,
  // extension_data->ServerNeedsUpdate() will be false after this
  // function is called.  This function leaves
  // extension_data->ClientNeedsUpdate() unchanged.
  bool UpdateServer(ExtensionData* extension_data,
                    sync_api::WriteTransaction* trans,
                    const sync_api::ReadNode& root);

  // Tries to update the client data from the given extension data.
  // extension_data->ServerNeedsUpdate() must not hold and
  // extension_data->ClientNeedsUpdate() must hold before this
  // function is called.  If the update was successful,
  // extension_data->ClientNeedsUpdate() will be false after this
  // function is called.  Otherwise, the extension needs updating to a
  // new version.
  void TryUpdateClient(ExtensionData* extension_data);

  // Kick off a run of the extension updater.
  //
  // TODO(akalin): Combine this with the similar function in
  // theme_util.cc.
  void NudgeExtensionUpdater();

  // Weak pointer.
  ProfileSyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_MODEL_ASSOCIATOR_H_
