// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_ABSTRACT_AUTOFILL_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_ABSTRACT_AUTOFILL_MODEL_ASSOCIATOR_H_

#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "net/base/escape.h"

class WebDatabase;

namespace browser_sync {

static const char kAutofillTag[] = "google_chrome_autofill";

// Contains all model association related logic:
// * Algorithm to associate autofill model and sync model.
// We do not check if we have local data before this run; we always
// merge and sync.
template <typename KeyType>
class AbstractAutofillModelAssociator
    : public PerDataTypeAssociatorInterface<KeyType, KeyType> {
 public:
  AbstractAutofillModelAssociator(ProfileSyncService* sync_service,
                                  WebDatabase* web_database,
                                  UnrecoverableErrorHandler* error_handler)
      : sync_service_(sync_service),
        web_database_(web_database),
        error_handler_(error_handler) {
    DCHECK(sync_service_);
    DCHECK(web_database_);
    DCHECK(error_handler_);
  }
  virtual ~AbstractAutofillModelAssociator() {}

  //
  // Iterates through the sync model looking for matched pairs of items.
  virtual bool AssociateModels() = 0;

  // Clears all associations.
  virtual bool DisassociateModels() {
    id_map_.clear();
    id_map_inverse_.clear();
    return true;
  }

  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) {
    DCHECK(has_nodes);
    *has_nodes = false;
    int64 autofill_sync_id;
    if (!GetSyncIdForTaggedNode(kAutofillTag, &autofill_sync_id)) {
      LOG(ERROR) << "Server did not create the top-level autofill node. We "
                 << "might be running against an out-of-date server.";
      return false;
    }
    sync_api::ReadTransaction trans(
        sync_service_->backend()->GetUserShareHandle());

    sync_api::ReadNode autofill_node(&trans);
    if (!autofill_node.InitByIdLookup(autofill_sync_id)) {
      LOG(ERROR) << "Server did not create the top-level autofill node. We "
                 << "might be running against an out-of-date server.";
      return false;
    }

    // The sync model has user created nodes if the autofill folder has any
    // children.
    *has_nodes = sync_api::kInvalidId != autofill_node.GetFirstChildId();
    return true;
  }

  // The has_nodes out param is true if the autofill model has any
  // user-defined autofill entries.
  virtual bool ChromeModelHasUserCreatedNodes(bool* has_nodes) {
    DCHECK(has_nodes);
    // Assume the autofill model always have user-created nodes.
    *has_nodes = true;
    return true;
  }

  // Returns the sync id for the given autofill name, or sync_api::kInvalidId
  // if the autofill name is not associated to any sync id.
  virtual int64 GetSyncIdFromChromeId(KeyType autofill) {
    typename AutofillToSyncIdMap::const_iterator iter = id_map_.find(autofill);
    return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
  }

  // Associates the given autofill name with the given sync id.
  virtual void Associate(const KeyType* autofill, int64 sync_id) {
    DCHECK_NE(sync_api::kInvalidId, sync_id);
    DCHECK(id_map_.find(*autofill) == id_map_.end());
    DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
    id_map_[*autofill] = sync_id;
    id_map_inverse_[sync_id] = *autofill;
  }

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id) {
    typename SyncIdToAutofillMap::iterator iter =
        id_map_inverse_.find(sync_id);
    if (iter == id_map_inverse_.end())
      return;
    CHECK(id_map_.erase(iter->second));
    id_map_inverse_.erase(iter);
  }

  // Returns whether a node with the given permanent tag was found and update
  // |sync_id| with that node's id.
  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id) {
    sync_api::ReadTransaction trans(
        sync_service_->backend()->GetUserShareHandle());
    sync_api::ReadNode sync_node(&trans);
    if (!sync_node.InitByTagLookup(tag.c_str()))
      return false;
    *sync_id = sync_node.GetId();
    return true;
  }

  // Not implemented.
  virtual const KeyType* GetChromeNodeFromSyncId(int64 sync_id) {
    return NULL;
  }

  // Not implemented.
  virtual bool InitSyncNodeFromChromeId(KeyType node_id,
                                        sync_api::BaseNode* sync_node) {
    return false;
  }

 protected:
  typedef std::map<KeyType, int64> AutofillToSyncIdMap;
  typedef std::map<int64, KeyType> SyncIdToAutofillMap;

  ProfileSyncService* sync_service_;
  WebDatabase* web_database_;
  UnrecoverableErrorHandler* error_handler_;

  AutofillToSyncIdMap id_map_;
  SyncIdToAutofillMap id_map_inverse_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractAutofillModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_ABSTRACT_AUTOFILL_MODEL_ASSOCIATOR_H_

