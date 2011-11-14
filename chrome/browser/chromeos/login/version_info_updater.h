// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_INFO_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_INFO_UPDATER_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"

namespace chromeos {

// Fetches all info we want to show on OOBE/Login screens about system
// version, boot times and cloud policy.
class VersionInfoUpdater : policy::CloudPolicySubsystem::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when OS version label should be updated.
    virtual void OnOSVersionLabelTextUpdated(
        const std::string& os_version_label_text) = 0;

    // Called when boot times label should be updated.
    virtual void OnBootTimesLabelTextUpdated(
        const std::string& boot_times_label_text) = 0;
  };

  explicit VersionInfoUpdater(Delegate* delegate);
  virtual ~VersionInfoUpdater();

  // Sets delegate.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Starts fetching version info. The delegate will be notified when update
  // is received.
  void StartUpdate(bool is_official_build);

 private:
  // policy::CloudPolicySubsystem::Observer methods:
  virtual void OnPolicyStateChanged(
      policy::CloudPolicySubsystem::PolicySubsystemState state,
      policy::CloudPolicySubsystem::ErrorDetails error_details) OVERRIDE;

  // Update the version label.
  void UpdateVersionLabel();

  // Check and update enterprise domain.
  void UpdateEnterpriseInfo();

  // Set enterprise domain name.
  void SetEnterpriseInfo(const std::string& domain_name,
                         const std::string& status_text);

  // Callback from chromeos::VersionLoader giving the version.
  void OnVersion(VersionLoader::Handle handle, std::string version);
  // Callback from chromeos::InfoLoader giving the boot times.
  void OnBootTimes(
      BootTimesLoader::Handle handle, BootTimesLoader::BootTimes boot_times);
  // Null callback from chromeos::InfoLoader.
  void OnBootTimesNoop(
      BootTimesLoader::Handle handle, BootTimesLoader::BootTimes boot_times);

  // Handles asynchronously loading the version.
  VersionLoader version_loader_;
  // Used to request the version.
  CancelableRequestConsumer version_consumer_;

  // Handles asynchronously loading the boot times.
  BootTimesLoader boot_times_loader_;
  // Used to request the boot times.
  CancelableRequestConsumer boot_times_consumer_;

  // Information pieces for version label.
  std::string version_text_;
  std::string enterprise_domain_text_;
  std::string enterprise_status_text_;

  // Full text for the OS version label.
  std::string os_version_label_text_;

  // CloudPolicySubsysterm observer registrar
  scoped_ptr<policy::CloudPolicySubsystem::ObserverRegistrar>
      cloud_policy_registrar_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(VersionInfoUpdater);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_INFO_UPDATER_H_
