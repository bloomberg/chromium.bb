// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/shutdown_policy_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"

namespace chromeos {

ShutdownPolicyObserver::ShutdownPolicyObserver(CrosSettings* cros_settings,
                                               Delegate* delegate)
    : cros_settings_(cros_settings), delegate_(delegate), weak_factory_(this) {
  if (delegate_) {
    shutdown_policy_subscription_ = cros_settings_->AddSettingsObserver(
        kRebootOnShutdown,
        base::Bind(&ShutdownPolicyObserver::OnShutdownPolicyChanged,
                   weak_factory_.GetWeakPtr()));
  }
}

ShutdownPolicyObserver::~ShutdownPolicyObserver() {
}

void ShutdownPolicyObserver::Shutdown() {
  shutdown_policy_subscription_.reset();
  delegate_ = nullptr;
}

void ShutdownPolicyObserver::CallDelegate(bool reboot_on_shutdown) {
  if (delegate_)
    delegate_->OnShutdownPolicyChanged(reboot_on_shutdown);
}

void ShutdownPolicyObserver::OnShutdownPolicyChanged() {
  CheckIfRebootOnShutdown(base::Bind(&ShutdownPolicyObserver::CallDelegate,
                          weak_factory_.GetWeakPtr()));
}

void ShutdownPolicyObserver::CheckIfRebootOnShutdown(
    const RebootOnShutdownCallback& callback) {
  CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::Bind(&ShutdownPolicyObserver::CheckIfRebootOnShutdown,
                     weak_factory_.GetWeakPtr(), callback));
  if (status != CrosSettingsProvider::TRUSTED)
    return;

  // Get the updated policy.
  bool reboot_on_shutdown = false;
  cros_settings_->GetBoolean(kRebootOnShutdown, &reboot_on_shutdown);
  callback.Run(reboot_on_shutdown);
}

}  // namespace chromeos
