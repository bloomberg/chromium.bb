// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_HELIUM_HELIUM_SYNC_MANAGER_H_
#define BLIMP_NET_HELIUM_HELIUM_SYNC_MANAGER_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"

namespace blimp {

class HeliumObject;
class HeliumTransport;
class Syncable;

using HeliumObjectId = uint32_t;

// TODO(kmarshall): Define this type.
class HeliumTransport {};

class HeliumSyncManager {
 public:
  // RAII object for managing the sync control and registration status of a
  // Syncable. When a Syncable is registered, it should hold onto the resulting
  // SyncRegistration object.
  //
  // When the Syncable is destroyed, it will delete the
  // SyncRegistration along with it, which will automatically unregister the
  // object from the SyncManager.
  //
  // If a Syncable wishes to halt synchronization for any reason (e.g. a tab
  // is hidden on the Client), it may call SyncRegistration::Pause(), which will
  // tell the SyncManager to exclude or include the Syncable in state sync.
  class SyncRegistration {
   public:
    explicit SyncRegistration(HeliumSyncManager* sync_manager,
                              HeliumObjectId id);
    ~SyncRegistration();

    // Tells the HeliumSyncManager to pause or unpause synchronization for the
    // HeliumObject associated with |this|.
    void Pause(bool paused);

    HeliumObjectId id() { return id_; }

   private:
    HeliumObjectId id_;
    HeliumSyncManager* sync_manager_;

    DISALLOW_COPY_AND_ASSIGN(SyncRegistration);
  };

  virtual ~HeliumSyncManager() {}

  // Returns a concrete implementation of HeliumSyncManager.
  static std::unique_ptr<HeliumSyncManager> Create(
      std::unique_ptr<HeliumTransport> transport);

  // Registers a new Syncable for synchronization. The Sync layer allocates a
  // unique identifier to the Object (including guaranteeing no clashes between
  // Client & Engine).
  // The identifier may be used by other Objects to refer to |object|. E.g. the
  // tab-list Object might include the Id for each tab’s Object.
  virtual std::unique_ptr<SyncRegistration> Register(Syncable* syncable) = 0;

  // Registers a Syncable for synchronization with a pre-existing ID.
  // This form is used when the remote peer has created a new Syncable, to
  // register our local equivalent.
  virtual std::unique_ptr<SyncRegistration> RegisterExisting(
      HeliumObjectId id,
      Syncable* syncable) = 0;

 protected:
  friend class HeliumSyncManager::SyncRegistration;

  // Tells the HeliumSyncManager to pause or unpause synchronization for the
  // HeliumObject associated with |this|.
  virtual void Pause(HeliumObjectId id, bool paused) = 0;

  // Indicates that a Syncable is being destroyed, and should no longer be
  // synchronized.
  // Because the Syncable is being destroyed, it is assumed that in-flight
  // messages
  // about it are now irrelevant, and the Sync layer should tear down underlying
  // resources, such as the HeliumStream, immediately.
  virtual void Unregister(HeliumObjectId id) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_HELIUM_HELIUM_SYNC_MANAGER_H_
