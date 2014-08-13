// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INVALIDATOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace invalidation {
class InvalidationService;
class TiclInvalidationService;
}

namespace policy {

class CloudPolicyInvalidator;

// This class provides invalidations for device policy via a
// |CloudPolicyInvalidator| backed by an |InvalidationService|. If a user with a
// connected invalidation service is logged in, that service is used to conserve
// server resources. If there are no logged-in users or none of them have
// connected invalidation services, a device-global |TiclInvalidationService| is
// spun up.
// The class monitors the status of all invalidation services and switches
// between them whenever the service currently in use disconnects or the
// device-global invalidation service can be replaced with another service that
// just connected.
class DeviceCloudPolicyInvalidator : public content::NotificationObserver {
 public:
  DeviceCloudPolicyInvalidator();
  virtual ~DeviceCloudPolicyInvalidator();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class DeviceCloudPolicyInvalidatorTest;

  // Helper that monitors the status of a single |InvalidationService|.
  class InvalidationServiceObserver;

  // Status updates received from |InvalidationServiceObserver|s.
  void OnInvalidationServiceConnected(
      invalidation::InvalidationService* invalidation_service);
  void OnInvalidationServiceDisconnected(
      invalidation::InvalidationService* invalidation_service);

  // Attempt to create a |CloudPolicyInvalidator| backed by a connected
  // invalidation service. If there is no connected invalidation service, a
  // |CloudPolicyInvalidator| will be created later when a connected service
  // becomes available.
  // Further ensures that a device-global invalidation service is running iff
  // there is no other connected service.
  void TryToCreateInvalidator();

  // Create a |CloudPolicyInvalidator| backed by the |invalidation_service|.
  void CreateInvalidator(
      invalidation::InvalidationService* invalidation_service);

  // Destroy the current |CloudPolicyInvalidator|, if any.
  void DestroyInvalidator();

  // Destroy the device-global invalidation service, if any.
  void DestroyDeviceInvalidationService();

  content::NotificationRegistrar registrar_;

  // Device-global invalidation service.
  scoped_ptr<invalidation::TiclInvalidationService>
      device_invalidation_service_;

  // State observer for the device-global invalidation service.
  scoped_ptr<InvalidationServiceObserver> device_invalidation_service_observer_;

  // State observers for logged-in users' invalidation services.
  ScopedVector<InvalidationServiceObserver>
      profile_invalidation_service_observers_;

  // The invalidation service backing the current |CloudPolicyInvalidator|. NULL
  // if no |CloudPolicyInvalidator| exists.
  invalidation::InvalidationService* invalidation_service_;

  // The highest invalidation version that was handled already.
  int64 highest_handled_invalidation_version_;

  // The current |CloudPolicyInvalidator|. NULL if no connected invalidation
  // service is available.
  scoped_ptr<CloudPolicyInvalidator> invalidator_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyInvalidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INVALIDATOR_H_
