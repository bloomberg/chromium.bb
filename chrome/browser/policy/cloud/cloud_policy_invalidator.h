// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "sync/notifier/invalidation_handler.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace invalidation {
class InvalidationService;
}

namespace policy {

class CloudPolicyInvalidationHandler;

// Listens for and provides policy invalidations.
class CloudPolicyInvalidator : public syncer::InvalidationHandler,
                               public CloudPolicyStore::Observer {
 public:
  // The number of minutes to delay a policy refresh after receiving an
  // invalidation with no payload.
  static const int kMissingPayloadDelay;

  // The default, min and max values for max_fetch_delay_.
  static const int kMaxFetchDelayDefault;
  static const int kMaxFetchDelayMin;
  static const int kMaxFetchDelayMax;

  // |invalidation_handler| handles invalidations provided by this object and
  // must remain valid until Shutdown is called.
  // |store| is cloud policy store. It must remain valid until Shutdown is
  // called.
  // |task_runner| is used for scheduling delayed tasks. It must post tasks to
  // the main policy thread.
  CloudPolicyInvalidator(
      CloudPolicyInvalidationHandler* invalidation_handler,
      CloudPolicyStore* store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  virtual ~CloudPolicyInvalidator();

  // Initializes the invalidator with the given profile. The invalidator uses
  // the profile to get a reference to the profile's invalidation service if
  // needed. Both the profile and the invalidation service must remain valid
  // until Shutdown is called. An Initialize method must only be called once.
  void InitializeWithProfile(Profile* profile);

  // Initializes the invalidator with the invalidation service. It must remain
  // valid until Shutdown is called. An Initialize method must only be called
  // once.
  void InitializeWithService(
      invalidation::InvalidationService* invalidation_service);

  // Shuts down and disables invalidations. It must be called before the object
  // is destroyed.
  void Shutdown();

  // Whether the invalidator currently has the ability to receive invalidations.
  bool invalidations_enabled() {
    return invalidations_enabled_;
  }

  // syncer::InvalidationHandler:
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 protected:
  // Allows subclasses to create a weak pointer to the object. The pointer
  // should only be used to call one of the Initialize methods, as after the
  // object is initialized weak pointers may be invalidated at any time.
  base::WeakPtr<CloudPolicyInvalidator> GetWeakPtr();

 private:
  // Initialize the invalidator.
  void Initialize();

  // Returns whether an Initialize method has been called.
  bool IsInitialized();

  // Handle an invalidation to the policy.
  void HandleInvalidation(const syncer::Invalidation& invalidation);

  // Update object registration with the invalidation service based on the
  // given policy data.
  void UpdateRegistration(const enterprise_management::PolicyData* policy);

  // Registers the given object with the invalidation service.
  void Register(int64 timestamp, const invalidation::ObjectId& object_id);

  // Unregisters the current object with the invalidation service.
  void Unregister();

  // Update |max_fetch_delay_| based on the given policy map.
  void UpdateMaxFetchDelay(const PolicyMap& policy_map);
  void set_max_fetch_delay(int delay);

  // Updates invalidations_enabled_ and calls the invalidation handler if the
  // value changed.
  void UpdateInvalidationsEnabled();

  // Run the invalidate callback on the invalidation handler. is_missing_payload
  // is set to true if the callback is being invoked in response to an
  // invalidation with a missing payload.
  void RunInvalidateCallback(bool is_missing_payload);

  // Acknowledge the latest invalidation.
  void AcknowledgeInvalidation();

  // Get the kMetricPolicyRefresh histogram metric which should be incremented
  // when a policy is stored.
  int GetPolicyRefreshMetric();

  // The handler for invalidations provded by this object.
  CloudPolicyInvalidationHandler* invalidation_handler_;

  // The cloud policy store.
  CloudPolicyStore* store_;

  // Schedules delayed tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The profile which will be used to get a reference to an invalidation
  // service.
  Profile* profile_;

  // The invalidation service.
  invalidation::InvalidationService* invalidation_service_;

  // Whether the invalidator currently has the ability to receive invalidations.
  // This is true if the invalidation service is enabled and the invalidator
  // has registered for a policy object.
  bool invalidations_enabled_;

  // Whether the invalidation service is currently enabled.
  bool invalidation_service_enabled_;

  // The timestamp of the PolicyData at which this object registered for policy
  // invalidations. Set to zero if the object has not registered yet.
  int64 registered_timestamp_;

  // The object id representing the policy in the invalidation service.
  invalidation::ObjectId object_id_;

  // Whether the policy is current invalid. This is set to true when an
  // invalidation is received and reset when the policy fetched due to the
  // invalidation is stored.
  bool invalid_;

  // The version of the latest invalidation received. This is compared to
  // the invalidation version of policy stored to determine when the
  // invalidated policy is up-to-date.
  int64 invalidation_version_;

  // The number of invalidations with unknown version received. Since such
  // invalidations do not provide a version number, this count is used to set
  // invalidation_version_ when such invalidations occur.
  int unknown_version_invalidation_count_;

  // The acknowledgment handle for the current invalidation.
  syncer::AckHandle ack_handle_;

  // WeakPtrFactory used to create callbacks to this object.
  base::WeakPtrFactory<CloudPolicyInvalidator> weak_factory_;

  // The maximum random delay, in ms, between receiving an invalidation and
  // fetching the new policy.
  int max_fetch_delay_;

  // A thread checker to make sure that callbacks are invoked on the correct
  // thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyInvalidator);
};

// Handles invalidations to cloud policy objects.
class CloudPolicyInvalidationHandler {
 public:
  virtual ~CloudPolicyInvalidationHandler() {}

  // This method is called when the current invalidation info should be set
  // on the cloud policy client.
  virtual void SetInvalidationInfo(
      int64 version,
      const std::string& payload) = 0;

  // This method is called when the policy should be refreshed due to an
  // invalidation. A policy fetch should be scheduled in the near future.
  virtual void InvalidatePolicy() = 0;

  // This method is called when the invalidator determines that the ability to
  // receive policy invalidations becomes enabled or disabled. The invalidator
  // starts in a disabled state, so the first call to this method is always when
  // the invalidator becomes enabled.
  virtual void OnInvalidatorStateChanged(bool invalidations_enabled) = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_INVALIDATOR_H_
