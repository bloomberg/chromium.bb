// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_invalidator.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "policy/proto/device_management_backend.pb.h"

namespace policy {

DeviceCloudPolicyInvalidator::DeviceCloudPolicyInvalidator(
    AffiliatedInvalidationServiceProvider* invalidation_service_provider)
    : invalidation_service_provider_(invalidation_service_provider),
      highest_handled_invalidation_version_(0) {
  invalidation_service_provider_->RegisterConsumer(this);
}

DeviceCloudPolicyInvalidator::~DeviceCloudPolicyInvalidator() {
  DestroyInvalidator();
  invalidation_service_provider_->UnregisterConsumer(this);
}

void DeviceCloudPolicyInvalidator::OnInvalidationServiceSet(
    invalidation::InvalidationService* invalidation_service) {
  DestroyInvalidator();
  if (invalidation_service)
    CreateInvalidator(invalidation_service);
}

CloudPolicyInvalidator*
DeviceCloudPolicyInvalidator::GetInvalidatorForTest() const {
  return invalidator_.get();
}

void DeviceCloudPolicyInvalidator::CreateInvalidator(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!invalidator_);
  invalidator_.reset(new CloudPolicyInvalidator(
      enterprise_management::DeviceRegisterRequest::DEVICE,
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetDeviceCloudPolicyManager()->core(),
      base::MessageLoopProxy::current(),
      scoped_ptr<base::Clock>(new base::DefaultClock()),
      highest_handled_invalidation_version_));
  invalidator_->Initialize(invalidation_service);
}

void DeviceCloudPolicyInvalidator::DestroyInvalidator() {
  if (!invalidator_)
    return;

  highest_handled_invalidation_version_ =
      invalidator_->highest_handled_invalidation_version();
  invalidator_->Shutdown();
  invalidator_.reset();
}

}  // namespace policy
