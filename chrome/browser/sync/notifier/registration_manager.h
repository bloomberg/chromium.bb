// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that manages the registration of types for server-issued
// notifications.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_REGISTRATION_MANAGER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_REGISTRATION_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace invalidation {
class InvalidationClient;
class ObjectId;
class RegistrationUpdateResult;
}  // namespace

namespace sync_notifier {

class RegistrationManager {
 public:
  // Does not take ownership of |invalidation_client_|.
  explicit RegistrationManager(
      invalidation::InvalidationClient* invalidation_client);

  ~RegistrationManager();

  // If |model_type| is valid, starts the process to register it and
  // returns true.  Otherwise, returns false.
  bool RegisterType(syncable::ModelType model_type);

  // Returns true iff |model_type| has been successfully registered.
  // Note that IsRegistered(model_type) may not immediately (or ever)
  // return true after calling RegisterType(model_type).
  //
  // Currently only used by unit tests.
  bool IsRegistered(syncable::ModelType model_type) const;

  // TODO(akalin): We will eventually need an UnregisterType().

  // Marks the registration for the |model_type| lost and re-registers
  // it.
  void MarkRegistrationLost(syncable::ModelType model_type);

  // Marks all registrations lost and re-registers them.
  void MarkAllRegistrationsLost();

 private:
  enum RegistrationStatus {
    // Registration request has not yet been sent.
    UNREGISTERED,
    // Registration request has been sent; waiting on confirmation.
    PENDING,
    // Registration has been confirmed.
    REGISTERED,
  };

  typedef std::map<syncable::ModelType, RegistrationStatus>
      RegistrationStatusMap;

  // Calls invalidation_client_->Register() on |object_id|.  sets
  // it->second to UNREGISTERED -> PENDING.
  void RegisterObject(const invalidation::ObjectId& object_id,
                      RegistrationStatusMap::iterator it);

  void OnRegister(const invalidation::RegistrationUpdateResult& result);

  NonThreadSafe non_thread_safe_;
  // Weak pointer.
  invalidation::InvalidationClient* invalidation_client_;
  RegistrationStatusMap registration_status_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationManager);
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_REGISTRATION_MANAGER_H_
