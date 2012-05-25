// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_STORE_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_STORE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace policy {

// Defines the low-level interface used by the cloud policy code to:
//   1. Validate policy blobs that should be applied locally
//   2. Persist policy blobs
//   3. Decode policy blobs to PolicyMap representation
class CloudPolicyStore {
 public:
  // Status codes.
  enum Status {
    // Everything is in good order.
    STATUS_OK,
    // Loading policy from the underlying data store failed.
    STATUS_PERSIST_LOAD_ERROR,
    // Failed to store policy to the data store.
    STATUS_PERSIST_STORE_ERROR,
    // Failed to parse the policy read from the data store.
    STATUS_PERSIST_PARSE_ERROR,
    // Failed to serialize policy for storage.
    STATUS_PERSIST_SERIALIZE_ERROR,
    // Validation failure: Bad signature.
    STATUS_VALIDATION_BAD_SIGNATURE,
    // Validation failure: Policy blob contains error code.
    STATUS_VALIDATION_ERROR_CODE_PRESENT,
    // Validation failure: Policy payload failed to decode.
    STATUS_VALIDATION_PAYLOAD_PARSE_ERROR,
    // Validation failure: Unexpected policy type.
    STATUS_VALIDATION_POLICY_TYPE,
    // Validation failure: Time stamp from the future.
    STATUS_VALIDATION_TIMESTAMP,
    // Validation failure: Token doesn't match.
    STATUS_VALIDATION_TOKEN,
    // Validation failure: Username doesn't match.
    STATUS_VALIDATION_USERNAME,
    // Policy protobuf parse error.
    STATUS_VALIDATION_POLICY_PARSE_ERROR,
  };

  // Callbacks for policy store events. Most importantly, policy updates.
  class Observer {
   public:
    virtual ~Observer();

    // Called on changes to store->policy() and/or store->policy_map().
    virtual void OnStoreLoaded(CloudPolicyStore* store) = 0;

    // Called upon encountering errors.
    virtual void OnStoreError(CloudPolicyStore* store) = 0;
  };

  CloudPolicyStore();
  virtual ~CloudPolicyStore();

  // Indicates whether the store has been fully initialized. This is
  // accomplished by calling Load() after startup.
  bool is_initialized() const { return is_initialized_; }

  const PolicyMap& policy_map() const { return policy_map_; }
  bool has_policy() const {
    return policy_.get() != NULL;
  }
  const enterprise_management::PolicyData* policy() const {
    return policy_.get();
  }
  bool is_managed() const {
    return policy_.get() &&
           policy_->state() == enterprise_management::PolicyData::ACTIVE;
  }
  Status status() const { return status_; }

  // Store a new policy blob. Pending store operations will be canceled. The
  // store operation may proceed asynchronously and observers are notified once
  // the operation finishes. If successful, OnStoreLoaded() will be invoked on
  // the observers and the updated policy can be read through policy(). Errors
  // generate OnStoreError() notifications.
  virtual void Store(
      const enterprise_management::PolicyFetchResponse& policy) = 0;

  // Load the current policy blob from persistent storage. This may trigger
  // asynchronous operations. Upon success, OnStoreLoaded() will be called on
  // the registered observers. Otherwise, OnStoreError() reports the reason for
  // failure.
  virtual void Load() = 0;

  // Registers an observer to be notified when policy changes.
  void AddObserver(Observer* observer);

  // Removes the specified observer.
  void RemoveObserver(Observer* observer);

 protected:
  // Invokes the corresponding callback on all registered observers.
  void NotifyStoreLoaded();
  void NotifyStoreError();

  // Decoded version of the currently effective policy.
  PolicyMap policy_map_;

  // Currently effective policy.
  scoped_ptr<enterprise_management::PolicyData> policy_;

  // Latest status code.
  Status status_;

 private:
  // Whether the store has completed asynchronous initialization, which is
  // triggered by calling Load().
  bool is_initialized_;

  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_STORE_H_
