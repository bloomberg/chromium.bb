// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SESSION_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_SESSION_MODEL_ASSOCIATOR_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/scoped_vector.h"
#include "base/string_util.h"
#include "base/threading/non_thread_safe.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/foreign_session_tracker.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/tab_contents/tab_contents.h"

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
class SessionModelAssociator
    : public PerDataTypeAssociatorInterface<TabContents, size_t>,
      public base::NonThreadSafe {
 public:
  // Does not take ownership of sync_service.
  explicit SessionModelAssociator(ProfileSyncService* sync_service);
  SessionModelAssociator(ProfileSyncService* sync_service,
                         bool setup_for_test);
  virtual ~SessionModelAssociator();

  // The has_nodes out parameter is set to true if the sync model has
  // nodes other than the permanent tagged nodes.  The method may
  // return false if an error occurred.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);

  // AssociatorInterface and PerDataTypeAssociator Interface implementation.
  virtual void AbortAssociation() {
    // No implementation needed, this associator runs on the main thread.
  }

  // Returns sync id for the given chrome model id.
  // Returns sync_api::kInvalidId if the sync node is not found for the given
  // chrome id.
  virtual int64 GetSyncIdFromChromeId(const size_t& id);

  // Returns sync id for the given session tag.
  // Returns sync_api::kInvalidId if the sync node is not found for the given
  // tag
  virtual int64 GetSyncIdFromSessionTag(const std::string& tag);

  // Not used.
  virtual const TabContents* GetChromeNodeFromSyncId(int64 sync_id);

  // Not used.
  virtual bool InitSyncNodeFromChromeId(const size_t& id,
                                        sync_api::BaseNode* sync_node);

  // Resync local window information. Updates the local sessions header node
  // with the status of open windows and the order of tabs they contain. Should
  // only be called for changes that affect a window, not a change within a
  // single tab.
  //
  // If |reload_tabs| is true, will also resync all tabs (same as calling
  // ReassociateTabs with a vector of all tabs).
  void ReassociateWindows(bool reload_tabs);

  // Loads and reassociates the local tabs referenced in |tabs|.
  void ReassociateTabs(const std::vector<TabContents*>& tabs);

  // Reassociates a single tab with the sync model. Will check if the tab
  // already is associated with a sync node and allocate one if necessary.
  void ReassociateTab(const TabContents& tab);

  // Associate a local tab and it's sync node. Will overwrite the contents of
  // the sync node with new specifics built from the tab.
  virtual void Associate(const TabContents* tab, int64 sync_id);

  // Looks up the specified sync node, and marks that tab as closed, then marks
  // the node as free and deletes association.
  virtual void Disassociate(int64 sync_id);

  // Load any foreign session info stored in sync db and update the sync db
  // with local client data. Processes/reuses any sync nodes owned by this
  // client and creates any further sync nodes needed to store local header and
  // tab info.
  virtual bool AssociateModels();

  // Initializes the given sync node from the given chrome node id.
  // Returns false if no sync node was found for the given chrome node id or
  // if the initialization of sync node fails.
  virtual bool InitSyncNodeFromChromeId(const std::string& id,
                                        sync_api::BaseNode* sync_node);

  // Clear local sync data buffers. Does not delete sync nodes to avoid
  // tombstones. TODO(zea): way to eventually delete orphaned nodes.
  virtual bool DisassociateModels();

  // Returns the tag used to uniquely identify this machine's session in the
  // sync model.
  inline const std::string& GetCurrentMachineTag() {
    DCHECK(!current_machine_tag_.empty());
    return current_machine_tag_;
  }

  // Load and associate window and tab data for a foreign session
  bool AssociateForeignSpecifics(const sync_pb::SessionSpecifics& specifics,
                                 int64 modification_time);

  // Removes a foreign session from our internal bookkeeping.
  void DisassociateForeignSession(const std::string& foreign_session_tag);

  // Builds a list of all foreign sessions.
  // Caller does NOT own ForeignSession objects.
  bool GetAllForeignSessions(std::vector<const ForeignSession*>* sessions);

  // Loads all windows for foreign session with session tag |tag|.
  // Caller does NOT own ForeignSession objects.
  bool GetForeignSession(const std::string& tag,
                         std::vector<SessionWindow*>* windows);

  // Looks up the foreign tab identified by |tab_id| and belonging to foreign
  // session |tag|.
  // Caller does NOT own the SessionTab object.
  bool GetForeignTab(const std::string& tag,
                     const SessionID::id_type tab_id,
                     const SessionTab** tab);

  // Specifies whether the window has tabs to sync. The new tab page does not
  // count. If no tabs to sync, it returns true, otherwise false;
  static bool SessionWindowHasNoTabsToSync(const SessionWindow& window);

  // Control which local tabs we're interested in syncing.
  // Ensures the profile matches sync's profile and that the tab has at least
  // one navigation entry and is not an empty tab.
  bool IsValidTab(const TabContents& tab);

  // Control which foreign tabs we're interested in displaying.
  // Checks that the tab has navigations and is not a new tab.
  // Note: a new tab page with back/forward history is valid.
  static bool IsValidSessionTab(const SessionTab& tab);

  // Returns the syncable model type.
  static syncable::ModelType model_type() { return syncable::SESSIONS; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, WriteSessionToNode);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest,
                           WriteFilledSessionToNode);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest,
                           WriteForeignSessionToNode);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, TabNodePoolEmpty);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, TabNodePoolNonEmpty);
  FRIEND_TEST_ALL_PREFIXES(SessionModelAssociatorTest, PopulateSessionWindow);
  FRIEND_TEST_ALL_PREFIXES(SessionModelAssociatorTest, PopulateSessionTab);

  // Keep all the links to local tab data in one place.
  class TabLinks {
   public:
    // To support usage as second value in maps we need default and copy
    // constructors.
    TabLinks()
        : sync_id_(0),
          session_tab_(NULL),
          tab_(NULL) {}

    // We only ever have either a SessionTab (for foreign tabs), or a
    // TabContents (for local tabs).
    TabLinks(int64 sync_id, const TabContents* tab)
      : sync_id_(sync_id),
        session_tab_(NULL) {
      tab_ = const_cast<TabContents*>(tab);
    }
    TabLinks(int64 sync_id, const SessionTab* session_tab)
      : sync_id_(sync_id),
        tab_(NULL) {
      session_tab_ = const_cast<SessionTab*>(session_tab);
    }

    inline int64 sync_id() const { return sync_id_; }
    inline const SessionTab* session_tab() const { return session_tab_; }
    inline const TabContents* tab() const { return tab_; }
   private:
    int64 sync_id_;
    SessionTab* session_tab_;
    TabContents* tab_;
  };

  // A pool for managing free/used tab sync nodes. Performs lazy creation
  // of sync nodes when necessary.
  class TabNodePool {
   public:
    explicit TabNodePool(ProfileSyncService* sync_service);
    ~TabNodePool();

    // Add a previously allocated tab sync node to our pool. Increases the size
    // of tab_syncid_pool_ by one and marks the new tab node as free.
    // Note: this should only be called when we discover tab sync nodes from
    // previous sessions, not for freeing tab nodes we created through
    // GetFreeTabNode (use FreeTabNode below for that).
    void AddTabNode(int64 sync_id);

    // Returns the sync_id for the next free tab node. If none are available,
    // creates a new tab node.
    // Note: We make use of the following "id's"
    // - a sync_id: an int64 used in |sync_api::InitByIdLookup|
    // - a tab_id: created by session service, unique to this client
    // - a tab_node_id: the id for a particular sync tab node. This is used
    //   to generate the sync tab node tag through:
    //       tab_tag = StringPrintf("%s_%ui", local_session_tag, tab_node_id);
    // tab_node_id and sync_id are both unique to a particular sync node. The
    // difference is that tab_node_id is controlled by the model associator and
    // is used when creating a new sync node, which returns the sync_id, created
    // by the sync db.
    int64 GetFreeTabNode();

    // Return a tab node to our free pool.
    // Note: the difference between FreeTabNode and AddTabNode is that
    // FreeTabNode does not modify the size of |tab_syncid_pool_|, while
    // AddTabNode increases it by one. In the case of FreeTabNode, the size of
    // the |tab_syncid_pool_| should always be equal to the amount of tab nodes
    // associated with this machine.
    void FreeTabNode(int64 sync_id);

    // Clear tab pool.
    inline void clear() { tab_syncid_pool_.clear(); }

    // Return the number of tab nodes this client currently has allocated
    // (including both free and used nodes)
    inline size_t capacity() const { return tab_syncid_pool_.size(); }

    // Return empty status (all tab nodes are in use).
    inline bool empty() const { return tab_pool_fp_ == -1; }

    // Return full status (no tab nodes are in use).
    inline bool full() {
      return tab_pool_fp_ == static_cast<int64>(tab_syncid_pool_.size())-1;
    }

    inline void set_machine_tag(const std::string& machine_tag) {
      machine_tag_ = machine_tag;
    }
   private:
    // Pool of all available syncid's for tab's we have created.
    std::vector<int64> tab_syncid_pool_;

    // Free pointer for tab pool. Only those node id's, up to and including the
    // one indexed by the free pointer, are valid and free. The rest of the
    // |tab_syncid_pool_| is invalid because the nodes are in use.
    // To get the next free node, use tab_syncid_pool_[tab_pool_fp_--].
    int64 tab_pool_fp_;

    // The machiine tag associated with this tab pool. Used in the title of new
    // sync nodes.
    std::string machine_tag_;

    // Our sync service profile (for making changes to the sync db)
    ProfileSyncService* sync_service_;

    DISALLOW_COPY_AND_ASSIGN(TabNodePool);
  };

  // Datatypes for accessing local tab data.
  typedef std::map<SessionID::id_type, TabLinks> TabLinksMap;

  // Delete all foreign session/window/tab objects allocated dynamically.
  // This is comprised of ForeignSession*, IDToSessionTabMap*, and any orphaned
  // SessionTab*'s.
  void DeleteForeignSessions();

  // Determine if a window is of a type we're interested in syncing.
  static bool ShouldSyncWindowType(const Browser::Type& type);

  // Build a sync tag from tab_node_id.
  static inline std::string TabIdToTag(
      const std::string machine_tag,
      size_t tab_node_id) {
    return StringPrintf("%s %"PRIuS"",
        machine_tag.c_str(), tab_node_id);
  }

  // Initializes the tag corresponding to this machine.
  void InitializeCurrentMachineTag(sync_api::WriteTransaction* trans);

  // Updates the server data based upon the current client session.  If no node
  // corresponding to this machine exists in the sync model, one is created.
  void UpdateSyncModelDataFromClient();

  // Pulls the current sync model from the sync database and returns true upon
  // update of the client model. Will associate any foreign sessions as well as
  // keep track of any local tab nodes, adding them to our free tab node pool.
  bool UpdateAssociationsFromSyncModel(const sync_api::ReadNode& root,
                                       const sync_api::BaseTransaction* trans);

  // Fills a tab sync node with data from a TabContents object.
  // (from a local navigation event)
  bool WriteTabContentsToSyncModel(const TabContents& tab,
                                   const int64 sync_id,
                                   sync_api::WriteTransaction* trans);

  // Used to populate a session window from the session specifics window
  // provided. Tracks any foreign session data created through |tracker|.
  static void PopulateSessionWindowFromSpecifics(
      std::string foreign_session_tag,
      const sync_pb::SessionWindow& window,
      const int64 mtime,
      SessionWindow* session_window,
      ForeignSessionTracker* tracker);

  // Used to populate a session tab from the session specifics tab provided.
  static void PopulateSessionTabFromSpecifics(const sync_pb::SessionTab& tab,
                                              const int64 mtime,
                                              SessionTab* session_tab);

  // Used to populate a session tab from the session specifics tab provided.
  static void AppendSessionTabNavigation(
     const sync_pb::TabNavigation& navigation,
     std::vector<TabNavigation>* navigations);

  // Populates the navigation portion of the session specifics.
  static void PopulateSessionSpecificsNavigation(
     const TabNavigation* navigation,
     sync_pb::TabNavigation* tab_navigation);

  // Returns the session service from |sync_service_|.
  SessionService* GetSessionService();

  // Internal method used in the callback to obtain the current session.
  // We don't own |windows|.
  void OnGotSession(int handle, std::vector<SessionWindow*>* windows);

  // Populate a session specifics header from a list of SessionWindows
  void PopulateSessionSpecificsHeader(
      const std::vector<SessionWindow*>& windows,
      sync_pb::SessionHeader* header_s);

  // Populates the window portion of the session specifics.
  void PopulateSessionSpecificsWindow(const SessionWindow& window,
                                      sync_pb::SessionWindow* session_window);

  // Syncs all the tabs in |window| with the local sync db. Will allocate tab
  // nodes if needed.
  bool SyncLocalWindowToSyncModel(const SessionWindow& window);

  // Fills a tab sync node with data from a SessionTab object.
  // (from ReadCurrentSessions)
  bool WriteSessionTabToSyncModel(const SessionTab& tab,
                                  const int64 sync_id,
                                  sync_api::WriteTransaction* trans);

  // Populates the tab portion of the session specifics.
  void PopulateSessionSpecificsTab(const SessionTab& tab,
                                   sync_pb::SessionTab* session_tab);

  // Local client name.
  std::string current_machine_tag_;

  // Pool of all used/available sync nodes associated with tabs.
  TabNodePool tab_pool_;

  // SyncID for the sync node containing all the window information for this
  // client.
  int64 local_session_syncid_;

  // Mapping of current open (local) tabs to their sync identifiers.
  TabLinksMap tab_map_;

  ForeignSessionTracker foreign_session_tracker_;

  // Weak pointer.
  ProfileSyncService* sync_service_;

  // Consumer used to obtain the current session.
  CancelableRequestConsumer consumer_;

  // To avoid certain checks not applicable to tests.
  bool setup_for_test_;

  DISALLOW_COPY_AND_ASSIGN(SessionModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SESSION_MODEL_ASSOCIATOR_H_
