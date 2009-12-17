// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"

namespace sync_api {
class BaseNode;
class BaseTransaction;
class ReadNode;
}

class ProfileSyncService;

namespace browser_sync {

// An enum identifying the various types of browser object models we know how
// to "associate" with the sync engine view of the world.
enum ModelType {
  MODEL_TYPE_BOOKMARKS,
};

// This represents the fundamental operations used for model association that
// are common to all ModelAssociators and do not depend on types of the models
// being associated.
class AssociatorInterface {
 public:
  virtual ~AssociatorInterface() {}

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

// In addition to the generic methods, association can refer to operations
// that depend on the types of the actual IDs we are associating and the
// underlying node type in the browser.  We collect these into a templatized
// interface that encapsulates everything you need to implement to have a model
// associator for a specific data type.
// This template is appropriate for data types where a Node* makes sense for
// referring to a particular item.  If we encounter a type that does not fit
// in this world, we may want to have several PerDataType templates.
template <class Node, class IDType>
class PerDataTypeAssociatorInterface : public AssociatorInterface {
 public:
  virtual ~PerDataTypeAssociatorInterface() {}
  // Returns sync id for the given chrome model id.
  // Returns sync_api::kInvalidId if the sync node is not found for the given
  // chrome id.
  virtual int64 GetSyncIdFromChromeId(IDType id) = 0;

  // Returns the chrome node for the given sync id.
  // Returns NULL if no node is found for the given sync id.
  virtual const Node* GetChromeNodeFromSyncId(int64 sync_id) = 0;

  // Initializes the given sync node from the given chrome node id.
  // Returns false if no sync node was found for the given chrome node id or
  // if the initialization of sync node fails.
  virtual bool InitSyncNodeFromChromeId(IDType node_id,
                                        sync_api::BaseNode* sync_node) = 0;

  // Associates the given chrome node with the given sync id.
  virtual void Associate(const Node* node, int64 sync_id) = 0;

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id) = 0;
};

// Contains all model association related logic to associate the chrome model
// with the sync model.
class ModelAssociator : public AssociatorInterface {
 public:
  explicit ModelAssociator(ProfileSyncService* sync_service)
      : sync_service_(sync_service) {
    DCHECK(sync_service);
  }
  ~ModelAssociator() { CleanupAllAssociators(); }

  // AssociatorInterface implementation. These are aggregated across all known
  // data types (for example, if both a Bookmarks and Preferences association
  // implementation is registered, AssociateModels() will associate both
  // Bookmarks and Preferences with the sync model).
  virtual bool AssociateModels() {
    bool success = true;
    for (ImplMap::iterator i = impl_map_.begin(); i != impl_map_.end(); ++i)
      success &= i->second->AssociateModels();
    return success;
  }

  virtual bool DisassociateModels() {
    bool success = true;
    for (ImplMap::iterator i = impl_map_.begin(); i != impl_map_.end(); ++i)
      success &= i->second->DisassociateModels();
    return success;
  }

  virtual bool SyncModelHasUserCreatedNodes() {
    for (ImplMap::iterator i = impl_map_.begin(); i != impl_map_.end(); ++i) {
      if (i->second->SyncModelHasUserCreatedNodes())
        return true;
    }
    return false;
  }

  virtual bool ChromeModelHasUserCreatedNodes() {
    for (ImplMap::iterator i = impl_map_.begin(); i != impl_map_.end(); ++i) {
      if (i->second->ChromeModelHasUserCreatedNodes())
        return true;
    }
    return false;
  }

  // Call to enable model association for a particular data type by registering
  // a type specific association implementation.  This will unregister any
  // previous implementation for the same model type, to enforce only one impl
  // for a given type at a time.
  template <class Impl>
  void CreateAndRegisterPerDataTypeImpl() {
    UnregisterPerDataTypeImpl<Impl>();
    impl_map_[Impl::model_type()] = new Impl(sync_service_);
  }

  // Call to disable model association for a particular data type by
  // unregistering the specific association implementation.
  template <class Impl>
  void UnregisterPerDataTypeImpl() {
    ImplMap::iterator it = impl_map_.find(Impl::model_type());
    if (it == impl_map_.end())
      return;
    delete it->second;
    impl_map_.erase(it);
  }

  template <class Impl>
  Impl* GetImpl() {
    ImplMap::const_iterator it = impl_map_.find(Impl::model_type());
    // The cast is safe because of the static model_type() pattern.
    return reinterpret_cast<Impl*>(it->second);
  }

  void CleanupAllAssociators() {
    STLDeleteValues(&impl_map_);
  }

 private:
  ProfileSyncService* sync_service_;

  typedef std::map<ModelType, AssociatorInterface*> ImplMap;
  ImplMap impl_map_;
  DISALLOW_COPY_AND_ASSIGN(ModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_H_
