// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_INFO_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_INFO_UPDATER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace chromeos {

class CrosSettings;

// Fetches all info we want to show on OOBE/Login screens about system
// version, boot times and cloud policy.
class VersionInfoUpdater : public policy::CloudPolicyStore::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when OS version label should be updated.
    virtual void OnOSVersionLabelTextUpdated(
        const std::string& os_version_label_text) = 0;

    // Called when the enterprise info notice should be updated.
    virtual void OnEnterpriseInfoUpdated(
        const std::string& enterprise_info) = 0;
  };

  explicit VersionInfoUpdater(Delegate* delegate);
  virtual ~VersionInfoUpdater();

  // Sets delegate.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Starts fetching version info. The delegate will be notified when update
  // is received.
  void StartUpdate(bool is_official_build);

 private:
  // policy::CloudPolicyStore::Observer interface:
  virtual void OnStoreLoaded(policy::CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(policy::CloudPolicyStore* store) OVERRIDE;

  // Update the version label.
  void UpdateVersionLabel();

  // Check and update enterprise domain.
  void UpdateEnterpriseInfo();

  // Set enterprise domain name.
  void SetEnterpriseInfo(const std::string& domain_name);

  // Callback from chromeos::VersionLoader giving the version.
  void OnVersion(const std::string& version);

  // Handles asynchronously loading the version.
  VersionLoader version_loader_;
  // Handles asynchronously loading the boot times.
  BootTimesLoader boot_times_loader_;
  // Used to request version and boot times.
  base::CancelableTaskTracker tracker_;

  // Information pieces for version label.
  std::string version_text_;

  // Full text for the OS version label.
  std::string os_version_label_text_;

  ScopedVector<CrosSettings::ObserverSubscription> subscriptions_;

  chromeos::CrosSettings* cros_settings_;

  Delegate* delegate_;

  // Weak pointer factory so we can give our callbacks for invocation
  // at a later time without worrying that they will actually try to
  // happen after the lifetime of this object.
  base::WeakPtrFactory<VersionInfoUpdater> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(VersionInfoUpdater);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_INFO_UPDATER_H_
