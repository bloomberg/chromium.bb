// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/helium/helium_sync_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace blimp {
namespace {

class HeliumSyncManagerImpl : public HeliumSyncManager {
 public:
  explicit HeliumSyncManagerImpl(std::unique_ptr<HeliumTransport> transport);
  ~HeliumSyncManagerImpl() override;

  // HeliumSyncManager implementation.
  std::unique_ptr<SyncRegistration> Register(Syncable* object) override;
  std::unique_ptr<SyncRegistration> RegisterExisting(HeliumObjectId id,
                                                     Syncable* object) override;
  void Unregister(HeliumObjectId id) override;
  void Pause(HeliumObjectId id, bool paused) override;

 private:
  std::unique_ptr<HeliumTransport> transport_;

  DISALLOW_COPY_AND_ASSIGN(HeliumSyncManagerImpl);
};

HeliumSyncManagerImpl::HeliumSyncManagerImpl(
    std::unique_ptr<HeliumTransport> transport)
    : transport_(std::move(transport)) {
  DCHECK(transport_);
}

HeliumSyncManagerImpl::~HeliumSyncManagerImpl() {}

std::unique_ptr<HeliumSyncManager::SyncRegistration>
HeliumSyncManagerImpl::Register(Syncable* object) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<HeliumSyncManager::SyncRegistration>
HeliumSyncManagerImpl::RegisterExisting(HeliumObjectId id, Syncable* object) {
  NOTIMPLEMENTED();
  return nullptr;
}

void HeliumSyncManagerImpl::Unregister(HeliumObjectId id) {
  NOTIMPLEMENTED();
}

void HeliumSyncManagerImpl::Pause(HeliumObjectId id, bool paused) {
  NOTIMPLEMENTED();
}

}  // namespace

// static
std::unique_ptr<HeliumSyncManager> HeliumSyncManager::Create(
    std::unique_ptr<HeliumTransport> transport) {
  return base::MakeUnique<HeliumSyncManagerImpl>(std::move(transport));
}

HeliumSyncManager::SyncRegistration::SyncRegistration(
    HeliumSyncManager* sync_manager,
    HeliumObjectId id)
    : id_(id), sync_manager_(sync_manager) {
  DCHECK(sync_manager);
}

HeliumSyncManager::SyncRegistration::~SyncRegistration() {
  sync_manager_->Unregister(id_);
}

void HeliumSyncManager::SyncRegistration::Pause(bool paused) {
  sync_manager_->Pause(id_, paused);
}

}  // namespace blimp
