// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that manages the registration of types for server-issued
// notifications.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_REGISTRATION_MANAGER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_REGISTRATION_MANAGER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/invalidation-client.h"

namespace sync_notifier {

class RegistrationManager {

  friend class RegistrationManagerTest;

  FRIEND_TEST_ALL_PREFIXES(RegistrationManagerTest, RegisterType);
  FRIEND_TEST_ALL_PREFIXES(RegistrationManagerTest, UnregisterType);
  FRIEND_TEST_ALL_PREFIXES(RegistrationManagerTest, MarkRegistrationLost);
  FRIEND_TEST_ALL_PREFIXES(RegistrationManagerTest, MarkAllRegistrationsLost);
 public:
  // Does not take ownership of |invalidation_client_|.
  explicit RegistrationManager(
      invalidation::InvalidationClient* invalidation_client);

  ~RegistrationManager();

  void SetRegisteredTypes(const syncable::ModelTypeSet& types);

  // Returns true iff |model_type| is currently registered.
  //
  // Currently only used by unit tests.
  bool IsRegistered(syncable::ModelType model_type) const;

  // Marks the registration for the |model_type| lost and re-registers
  // it.
  void MarkRegistrationLost(syncable::ModelType model_type);

  // Marks all registrations lost and re-registers them.
  void MarkAllRegistrationsLost();

 private:
  typedef std::map<syncable::ModelType, invalidation::RegistrationState>
      RegistrationStatusMap;

  // Registers the given |model_type|, which must be valid.
  void RegisterType(syncable::ModelType model_type);

  void UnregisterType(syncable::ModelType model_type);

  // Calls invalidation_client_->Register() on |object_id|.  sets
  // it->second to UNREGISTERED -> PENDING.
  void RegisterObject(const invalidation::ObjectId& object_id,
                      RegistrationStatusMap::iterator it);

  void OnRegister(const invalidation::RegistrationUpdateResult& result);

  base::NonThreadSafe non_thread_safe_;
  // Weak pointer.
  invalidation::InvalidationClient* invalidation_client_;
  RegistrationStatusMap registration_status_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationManager);
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_REGISTRATION_MANAGER_H_
