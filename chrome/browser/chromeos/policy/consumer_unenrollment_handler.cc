// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_unenrollment_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

ConsumerUnenrollmentHandler::ConsumerUnenrollmentHandler(
    chromeos::DeviceSettingsService* device_settings_service,
    ConsumerManagementService* consumer_management_service,
    DeviceCloudPolicyManagerChromeOS* device_cloud_policy_manager,
    chromeos::OwnerSettingsServiceChromeOS* owner_settings_service)
    : device_settings_service_(device_settings_service),
      consumer_management_service_(consumer_management_service),
      device_cloud_policy_manager_(device_cloud_policy_manager),
      owner_settings_service_(owner_settings_service),
      weak_factory_(this) {
}

ConsumerUnenrollmentHandler::~ConsumerUnenrollmentHandler() {
}

void ConsumerUnenrollmentHandler::Start() {
  if (consumer_management_service_->GetStatus() !=
      ConsumerManagementService::STATUS_ENROLLED) {
    return;
  }

  device_cloud_policy_manager_->Unregister(
      base::Bind(&ConsumerUnenrollmentHandler::OnUnregistered,
                 weak_factory_.GetWeakPtr()));
}

void ConsumerUnenrollmentHandler::OnUnregistered(bool success) {
  if (!success) {
    consumer_management_service_->SetStage(
        ConsumerManagementStage::UnenrollmentDMServerFailed());
    LOG(ERROR) << "Failed to unregister and disconnect device cloud policy "
               << "manager.";
    return;
  }


  chromeos::OwnerSettingsServiceChromeOS::ManagementSettings settings;
  settings.management_mode = MANAGEMENT_MODE_LOCAL_OWNER;
  owner_settings_service_->SetManagementSettings(
      settings,
      base::Bind(&ConsumerUnenrollmentHandler::OnManagementSettingsSet,
                 weak_factory_.GetWeakPtr()));
}

void ConsumerUnenrollmentHandler::OnManagementSettingsSet(bool success) {
  if (!success) {
    consumer_management_service_->SetStage(
        ConsumerManagementStage::UnenrollmentUpdateDeviceSettingsFailed());
    LOG(ERROR) << "Failed to unset request token and device ID.";
    return;
  }

  consumer_management_service_->SetStage(
      ConsumerManagementStage::UnenrollmentSuccess());

  // Disconnecting the device cloud policy manager will restart the device
  // cloud policy initializer. So this must be done after the management
  // settings are updated, so that the initializer won't reconnect the manager.
  device_cloud_policy_manager_->Disconnect();
}

}  // namespace policy
