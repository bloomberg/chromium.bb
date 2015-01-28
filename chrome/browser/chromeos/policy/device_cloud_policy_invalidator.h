// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INVALIDATOR_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider.h"

namespace invalidation {
class InvalidationService;
}

namespace policy {

class CloudPolicyInvalidator;

// This class manages the lifetime of a device-global |CloudPolicyInvalidator|
// that handles device policy invalidations. It relies on an
// |AffiliatedInvalidationServiceProvider| to provide it with access to a shared
// |InvalidationService| to back the |CloudPolicyInvalidator|. Whenever the
// shared |InvalidationService| changes, the |CloudPolicyInvalidator| destroyed
// and re-created.
class DeviceCloudPolicyInvalidator
    : public AffiliatedInvalidationServiceProvider::Consumer {
 public:
  explicit DeviceCloudPolicyInvalidator(
      AffiliatedInvalidationServiceProvider* invalidation_service_provider);
  ~DeviceCloudPolicyInvalidator() override;

  // AffiliatedInvalidationServiceProvider::Consumer:
  void OnInvalidationServiceSet(
      invalidation::InvalidationService* invalidation_service) override;

  CloudPolicyInvalidator* GetInvalidatorForTest() const;

 private:
  // Create a |CloudPolicyInvalidator| backed by the |invalidation_service|.
  void CreateInvalidator(
      invalidation::InvalidationService* invalidation_service);

  // Destroy the current |CloudPolicyInvalidator|, if any.
  void DestroyInvalidator();

  AffiliatedInvalidationServiceProvider* invalidation_service_provider_;

  // The highest invalidation version that was handled already.
  int64 highest_handled_invalidation_version_;

  // The current |CloudPolicyInvalidator|. nullptr if no connected invalidation
  // service is available.
  scoped_ptr<CloudPolicyInvalidator> invalidator_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyInvalidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INVALIDATOR_H_
