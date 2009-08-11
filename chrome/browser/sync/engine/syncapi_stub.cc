// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "base/logging.h"
#include "chrome/browser/sync/engine/syncapi.h"

namespace sync_api {

struct BaseNode::BaseNodeInternal {
};

struct UserShare {
};

class SyncInternal {
  DISALLOW_COPY_AND_ASSIGN(SyncInternal);
};

// BaseNode.
BaseNode::BaseNode() : data_(NULL) {}
BaseNode::~BaseNode() {}
int64 BaseNode::GetParentId() const {
  NOTIMPLEMENTED();
  return 0;
}

int64 BaseNode::GetId() const {
  NOTIMPLEMENTED();
  return 0;
}

bool BaseNode::GetIsFolder() const {
  NOTIMPLEMENTED();
  return false;
}

const sync_char16* BaseNode::GetTitle() const {
  NOTIMPLEMENTED();
  return NULL;
}

const sync_char16* BaseNode::GetURL() const {
  NOTIMPLEMENTED();
  return NULL;
}

const int64* BaseNode::GetChildIds(size_t* child_count) const {
  NOTIMPLEMENTED();
  return NULL;
}

int64 BaseNode::GetPredecessorId() const {
  NOTIMPLEMENTED();
  return 0;
}

int64 BaseNode::GetSuccessorId() const {
  NOTIMPLEMENTED();
  return 0;
}

int64 BaseNode::GetFirstChildId() const {
  NOTIMPLEMENTED();
  return 0;
}

const unsigned char* BaseNode::GetFaviconBytes(size_t* size_in_bytes) {
  NOTIMPLEMENTED();
  return NULL;
}

int64 BaseNode::GetExternalId() const {
  NOTIMPLEMENTED();
  return 0;
}


////////////////////////////////////
// WriteNode member definitions
void WriteNode::SetIsFolder(bool folder) {
  NOTIMPLEMENTED();
}

void WriteNode::SetTitle(const sync_char16* title) {
  NOTIMPLEMENTED();
}

void WriteNode::SetURL(const sync_char16* url) {
  NOTIMPLEMENTED();
}

void WriteNode::SetExternalId(int64 id) {
  NOTIMPLEMENTED();
}

WriteNode::WriteNode(WriteTransaction* transaction)
    : entry_(NULL), transaction_(NULL) {
  NOTIMPLEMENTED();
}

WriteNode::~WriteNode() {
  NOTIMPLEMENTED();
}

bool WriteNode::InitByIdLookup(int64 id) {
  NOTIMPLEMENTED();
  return false;
}

bool WriteNode::InitByCreation(const BaseNode& parent,
                               const BaseNode* predecessor) {
  NOTIMPLEMENTED();
  return false;
}

bool WriteNode::SetPosition(const BaseNode& new_parent,
                            const BaseNode* predecessor) {
  NOTIMPLEMENTED();
  return false;
}

const syncable::Entry* WriteNode::GetEntry() const {
  NOTIMPLEMENTED();
  return NULL;
}

const BaseTransaction* WriteNode::GetTransaction() const {
  NOTIMPLEMENTED();
  return NULL;
}

void WriteNode::Remove() {
  NOTIMPLEMENTED();
}

void WriteNode::PutPredecessor(const BaseNode* predecessor) {
  NOTIMPLEMENTED();
}

void WriteNode::SetFaviconBytes(const unsigned char* bytes,
                                size_t size_in_bytes) {
  NOTIMPLEMENTED();
}

void WriteNode::MarkForSyncing() {
  NOTIMPLEMENTED();
}

//////////////////////////////////////////////////////////////////////////
// ReadNode member definitions
ReadNode::ReadNode(const BaseTransaction* transaction)
    : entry_(NULL), transaction_(NULL) {
  NOTIMPLEMENTED();
}

ReadNode::~ReadNode() {
  NOTIMPLEMENTED();
}

void ReadNode::InitByRootLookup() {
  NOTIMPLEMENTED();
}

bool ReadNode::InitByIdLookup(int64 id) {
  NOTIMPLEMENTED();
  return false;
}

const syncable::Entry* ReadNode::GetEntry() const {
  NOTIMPLEMENTED();
  return NULL;
}

const BaseTransaction* ReadNode::GetTransaction() const {
  NOTIMPLEMENTED();
  return NULL;
}

bool ReadNode::InitByTagLookup(const sync_char16* tag) {
  NOTIMPLEMENTED();
  return false;
}

//////////////////////////////////////////////////////////////////////////
// ReadTransaction member definitions
ReadTransaction::ReadTransaction(UserShare* share) : BaseTransaction(NULL),
    transaction_(NULL) {
  NOTIMPLEMENTED();
}

ReadTransaction::~ReadTransaction() {
  NOTIMPLEMENTED();
}

syncable::BaseTransaction* ReadTransaction::GetWrappedTrans() const {
  NOTIMPLEMENTED();
  return NULL;
}

//////////////////////////////////////////////////////////////////////////
// WriteTransaction member definitions
WriteTransaction::WriteTransaction(UserShare* share) : BaseTransaction(NULL),
    transaction_(NULL) {
  NOTIMPLEMENTED();
}

WriteTransaction::~WriteTransaction() {
  NOTIMPLEMENTED();
}

syncable::BaseTransaction* WriteTransaction::GetWrappedTrans() const {
  NOTIMPLEMENTED();
  return NULL;
}

// SyncManager

SyncManager::SyncManager() {
  NOTIMPLEMENTED();
}

bool SyncManager::Init(const sync_char16* database_location,
                       const char* sync_server_and_path,
                       int sync_server_port,
                       const char* gaia_service_id,
                       const char* gaia_source,
                       bool use_ssl,
                       HttpPostProviderFactory* post_factory,
                       HttpPostProviderFactory* auth_post_factory,
                       ModelSafeWorkerInterface* model_safe_worker,
                       bool attempt_last_user_authentication,
                       const char* user_agent) {
  NOTIMPLEMENTED();
  return false;
}

void SyncManager::Authenticate(const char* username, const char* password) {
  NOTIMPLEMENTED();
}

const char* SyncManager::GetAuthenticatedUsername() {
  NOTIMPLEMENTED();
  return NULL;
}

SyncManager::~SyncManager() {
  NOTIMPLEMENTED();
}

void SyncManager::SetObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void SyncManager::RemoveObserver() {
  NOTIMPLEMENTED();
}

void SyncManager::Shutdown() {
  NOTIMPLEMENTED();
}

SyncManager::Status::Summary SyncManager::GetStatusSummary() const {
  NOTIMPLEMENTED();
  return SyncManager::Status::INVALID;
}

SyncManager::Status SyncManager::GetDetailedStatus() const {
  NOTIMPLEMENTED();
  return SyncManager::Status();
}

SyncManager::SyncInternal* SyncManager::GetImpl() const {
  NOTIMPLEMENTED();
  return NULL;
}

void SyncManager::SaveChanges() {
  NOTIMPLEMENTED();
}

void SyncManager::SetupForTestMode(const sync_char16* test_username) {
  NOTIMPLEMENTED();
}

//////////////////////////////////////////////////////////////////////////
// BaseTransaction member definitions
BaseTransaction::BaseTransaction(UserShare* share) : lookup_(NULL) {
  NOTIMPLEMENTED();
}
BaseTransaction::~BaseTransaction() {
  NOTIMPLEMENTED();
}

UserShare* SyncManager::GetUserShare() const {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace sync_api

#endif  // CHROME_PERSONALIZATION

