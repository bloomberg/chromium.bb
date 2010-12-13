// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SESSION_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_SESSION_MODEL_ASSOCIATOR_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/scoped_vector.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/common/notification_registrar.h"

class Profile;
class ProfileSyncService;

namespace sync_api {
class ReadNode;
class WriteNode;
class WriteTransaction;
}  // namespace sync_api

namespace sync_pb {
class SessionSpecifics;
}  // namespace sync_pb

namespace browser_sync {

static const char kSessionsTag[] = "google_chrome_sessions";

// Contains all logic for associating the Chrome sessions model and
// the sync sessions model.
// In the case of sessions, our local model is nothing but a buffer (specifics_)
// that gets overwritten everytime there is an update. From it, we build a new
// foreign session windows list each time |GetSessionData| is called by the
// ForeignSessionHandler.
class SessionModelAssociator : public PerDataTypeAssociatorInterface<
    sync_pb::SessionSpecifics, std::string>, public NonThreadSafe {
 public:

  // Does not take ownership of sync_service.
  explicit SessionModelAssociator(ProfileSyncService* sync_service);
  virtual ~SessionModelAssociator();

  // AssociatorInterface and PerDataTypeAssociator Interface implementation.
  virtual void AbortAssociation() {
    // No implementation needed, this associator runs on the main
    // thread.
  }

  // Dummy method, we do everything all-at-once in UpdateFromSyncModel.
  virtual void Associate(const sync_pb::SessionSpecifics* specifics,
    int64 sync_id) {
  }

  // Updates the sync model with the local client data. (calls
  // UpdateFromSyncModel)
  virtual bool AssociateModels();

  // The has_nodes out parameter is set to true if the chrome model
  // has user-created nodes.  The method may return false if an error
  // occurred.
  virtual bool ChromeModelHasUserCreatedNodes(bool* has_nodes);

  // Dummy method, we do everything all-at-once in UpdateFromSyncModel.
  virtual void Disassociate(int64 sync_id) {
  }

  // Clear specifics_ buffer and notify foreign session handlers.
  virtual bool DisassociateModels();

  // Returns the chrome session specifics for the given sync id.
  // Returns NULL if no specifics are found for the given sync id.
  virtual const sync_pb::SessionSpecifics* GetChromeNodeFromSyncId(
    int64 sync_id);

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  virtual bool GetSyncIdForTaggedNode(const std::string* tag, int64* sync_id);

  // Returns sync id for the given chrome model id.
  // Returns sync_api::kInvalidId if the sync node is not found for the given
  // chrome id.
  virtual int64 GetSyncIdFromChromeId(const std::string& id);


  // Initializes the given sync node from the given chrome node id.
  // Returns false if no sync node was found for the given chrome node id or
  // if the initialization of sync node fails.
  virtual bool InitSyncNodeFromChromeId(const std::string& id,
                                        sync_api::BaseNode* sync_node);

  // The has_nodes out parameter is set to true if the sync model has
  // nodes other than the permanent tagged nodes.  The method may
  // return false if an error occurred.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);

  // Returns the tag used to uniquely identify this machine's session in the
  // sync model.
  std::string GetCurrentMachineTag();

  // Updates the server data based upon the current client session.  If no node
  // corresponding to this machine exists in the sync model, one is created.
  void UpdateSyncModelDataFromClient();

  // Pulls the current sync model from the sync database and returns true upon
  // update of the client model. Called by SessionChangeProcessor.
  // Note that the specifics_ vector is reset and built from scratch each time.
  bool UpdateFromSyncModel(const sync_api::BaseTransaction* trans);

  // Helper for UpdateFromSyncModel. Appends sync data to a vector of specifics.
  bool QuerySyncModel(const sync_api::BaseTransaction* trans,
      std::vector<const sync_pb::SessionSpecifics*>& specifics);

  // Builds sessions from buffered specifics data
  bool GetSessionData(std::vector<ForeignSession*>* sessions);

  // Helper method to generate session specifics from session windows.
  void FillSpecificsFromSessions(std::vector<SessionWindow*>* windows,
      sync_pb::SessionSpecifics* session);

  // Helper method for converting session specifics to windows.
  void AppendForeignSessionFromSpecifics(
      const sync_pb::SessionSpecifics* specifics,
      std::vector<ForeignSession*>* session);

  // Fills the given empty vector with foreign session windows to restore.
  // If the vector is returned empty, then the session data could not be
  // converted back into windows.
  void AppendForeignSessionWithID(int64 id,
      std::vector<ForeignSession*>* session,
      sync_api::BaseTransaction* trans);

  // Returns the syncable model type.
  static syncable::ModelType model_type() { return syncable::SESSIONS; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, WriteSessionToNode);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest,
                           WriteForeignSessionToNode);

  // Returns the session service from |sync_service_|.
  SessionService* GetSessionService();

  // Initializes the tag corresponding to this machine.
  // Note: creates a syncable::BaseTransaction.
  void InitializeCurrentMachineTag();

  // Populates the navigation portion of the session specifics.
  void PopulateSessionSpecificsNavigation(const TabNavigation* navigation,
      sync_pb::TabNavigation* tab_navigation);

  // Populates the tab portion of the session specifics.
  void PopulateSessionSpecificsTab(const SessionTab* tab,
      sync_pb::SessionTab* session_tab);

  // Populates the window portion of the session specifics.
  void PopulateSessionSpecificsWindow(const SessionWindow* window,
      sync_pb::SessionWindow* session_window);

  // Specifies whether the window has tabs to sync.  The new tab page does not
  // count.  If no tabs to sync, it returns true, otherwise false;
  bool WindowHasNoTabsToSync(const SessionWindow* window);

  // Internal method used in the callback to obtain the current session.
  // We don't own |windows|.
  void OnGotSession(int handle, std::vector<SessionWindow*>* windows);

  // Used to populate a session tab from the session specifics tab provided.
  void AppendSessionTabNavigation(std::vector<TabNavigation>* navigations,
      const sync_pb::TabNavigation* navigation);

  // Used to populate a session tab from the session specifics tab provided.
  void PopulateSessionTabFromSpecifics(SessionTab* session_tab,
      const sync_pb::SessionTab* tab, SessionID id);

  // Used to populate a session window from the session specifics window
  // provided.
  void PopulateSessionWindowFromSpecifics(SessionWindow* session_window,
      const sync_pb::SessionWindow* window);

  // Updates the current session on the server.  Creates a node for this machine
  // if there is not one already.
  bool UpdateSyncModel(sync_pb::SessionSpecifics* session_data,
                    sync_api::WriteTransaction* trans,
                    const sync_api::ReadNode* root);
  // Stores the machine tag.
  std::string current_machine_tag_;

  // Stores the current set of foreign session specifics.
  // Used by ForeignSessionHandler through |GetSessionData|.
  // Built by |QuerySyncModel| via |UpdateFromSyncModel|.
  std::vector<const sync_pb::SessionSpecifics*> specifics_;

  // Weak pointer.
  ProfileSyncService* sync_service_;

  // Consumer used to obtain the current session.
  CancelableRequestConsumer consumer_;

  DISALLOW_COPY_AND_ASSIGN(SessionModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SESSION_MODEL_ASSOCIATOR_H_
