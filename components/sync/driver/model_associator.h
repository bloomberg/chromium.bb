// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_MODEL_ASSOCIATOR_H_
#define COMPONENTS_SYNC_DRIVER_MODEL_ASSOCIATOR_H_

#include <stdint.h>

#include "base/synchronization/lock.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_error.h"

namespace syncer {

class BaseNode;
class SyncMergeResult;

// This represents the fundamental operations used for model association that
// are common to all ModelAssociators and do not depend on types of the models
// being associated.
class AssociatorInterface {
 public:
  virtual ~AssociatorInterface() {}

  // Iterates through both the sync and the chrome model looking for
  // matched pairs of items. After successful completion, the models
  // should be identical and corresponding. Returns true on
  // success. On failure of this step, we should abort the sync
  // operation and report an error to the user.
  virtual SyncError AssociateModels(SyncMergeResult* local_merge_result,
                                    SyncMergeResult* syncer_merge_result) = 0;

  // Clears all the associations between the chrome and sync models.
  virtual SyncError DisassociateModels() = 0;

  // The has_nodes out parameter is set to true if the sync model has
  // nodes other than the permanent tagged nodes.  The method may
  // return false if an error occurred.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) = 0;

  // Calling this method while AssociateModels() is in progress will
  // cause the method to exit early with a "false" return value.  This
  // is useful for aborting model associations for shutdown.  This
  // method is only implemented for model associators that are invoked
  // off the main thread.
  virtual void AbortAssociation() = 0;

  // Returns whether the datatype is ready for encryption/decryption if the
  // sync service requires it.
  // TODO(zea): This should be implemented automatically for each datatype, see
  // http://crbug.com/76232.
  virtual bool CryptoReadyIfNecessary() = 0;
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
  // Returns kInvalidId if the sync node is not found for the given
  // chrome id.
  virtual int64_t GetSyncIdFromChromeId(const IDType& id) = 0;

  // Returns the chrome node for the given sync id.
  // Returns null if no node is found for the given sync id.
  virtual const Node* GetChromeNodeFromSyncId(int64_t sync_id) = 0;

  // Initializes the given sync node from the given chrome node id.
  // Returns false if no sync node was found for the given chrome node id or
  // if the initialization of sync node fails.
  virtual bool InitSyncNodeFromChromeId(const IDType& node_id,
                                        BaseNode* sync_node) = 0;

  // Associates the given chrome node with the given sync node.
  virtual void Associate(const Node* node, const BaseNode& sync_node) = 0;

  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64_t sync_id) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_MODEL_ASSOCIATOR_H_
