// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_TPM_AUTO_UPDATE_MODE_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_TPM_AUTO_UPDATE_MODE_POLICY_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace policy {

// This class observes the device setting |kTPMFirmwareUpdateSettings| and
// starts the TPM firmware auto-update flow according to its value.
class TPMAutoUpdateModePolicyHandler {
 public:
  explicit TPMAutoUpdateModePolicyHandler(
      chromeos::CrosSettings* cros_settings);
  ~TPMAutoUpdateModePolicyHandler();

 private:
  void OnPolicyChanged();

  chromeos::CrosSettings* cros_settings_;

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      policy_subscription_;

  base::WeakPtrFactory<TPMAutoUpdateModePolicyHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TPMAutoUpdateModePolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_TPM_AUTO_UPDATE_MODE_POLICY_HANDLER_H_
