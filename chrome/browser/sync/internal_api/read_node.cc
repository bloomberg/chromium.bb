// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/internal_api/read_node.h"

#include "base/logging.h"
#include "chrome/browser/sync/internal_api/base_transaction.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace sync_api {

//////////////////////////////////////////////////////////////////////////
// ReadNode member definitions
ReadNode::ReadNode(const BaseTransaction* transaction)
    : entry_(NULL), transaction_(transaction) {
  DCHECK(transaction);
}

ReadNode::ReadNode() {
  entry_ = NULL;
  transaction_ = NULL;
}

ReadNode::~ReadNode() {
  delete entry_;
}

void ReadNode::InitByRootLookup() {
  DCHECK(!entry_) << "Init called twice";
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_ID, trans->root_id());
  if (!entry_->good())
    DCHECK(false) << "Could not lookup root node for reading.";
}

bool ReadNode::InitByIdLookup(int64 id) {
  DCHECK(!entry_) << "Init called twice";
  DCHECK_NE(id, kInvalidId);
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_HANDLE, id);
  if (!entry_->good())
    return false;
  if (entry_->Get(syncable::IS_DEL))
    return false;
  syncable::ModelType model_type = GetModelType();
  LOG_IF(WARNING, model_type == syncable::UNSPECIFIED ||
                  model_type == syncable::TOP_LEVEL_FOLDER)
      << "SyncAPI InitByIdLookup referencing unusual object.";
  return DecryptIfNecessary();
}

bool ReadNode::InitByClientTagLookup(syncable::ModelType model_type,
                                     const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return false;

  const std::string hash = GenerateSyncableHash(model_type, tag);

  entry_ = new syncable::Entry(transaction_->GetWrappedTrans(),
                               syncable::GET_BY_CLIENT_TAG, hash);
  return (entry_->good() && !entry_->Get(syncable::IS_DEL) &&
          DecryptIfNecessary());
}

const syncable::Entry* ReadNode::GetEntry() const {
  return entry_;
}

const BaseTransaction* ReadNode::GetTransaction() const {
  return transaction_;
}

bool ReadNode::InitByTagLookup(const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return false;
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_SERVER_TAG, tag);
  if (!entry_->good())
    return false;
  if (entry_->Get(syncable::IS_DEL))
    return false;
  syncable::ModelType model_type = GetModelType();
  LOG_IF(WARNING, model_type == syncable::UNSPECIFIED ||
                  model_type == syncable::TOP_LEVEL_FOLDER)
      << "SyncAPI InitByTagLookup referencing unusually typed object.";
  return DecryptIfNecessary();
}

}  // namespace sync_api
