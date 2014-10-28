// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace chromeos {

DeviceDisabledScreen::DeviceDisabledScreen(
    BaseScreenDelegate* base_screen_delegate,
    DeviceDisabledScreenActor* actor)
    : BaseScreen(base_screen_delegate),
      showing_(false),
      actor_(actor),
      weak_factory_(this) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

DeviceDisabledScreen::~DeviceDisabledScreen() {
  if (actor_)
    actor_->SetDelegate(nullptr);
}

void DeviceDisabledScreen::PrepareToShow() {
}

void DeviceDisabledScreen::Show() {
  if (!actor_ || showing_)
    return;

  bool is_device_disabled = false;
  g_browser_process->local_state()->GetDictionary(
      prefs::kServerBackedDeviceState)->GetBoolean(policy::kDeviceStateDisabled,
                                                   &is_device_disabled);
  if (!is_device_disabled ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDeviceDisabling)) {
    // Skip the screen if the device is not marked as disabled or device
    // disabling has been turned off by flag.
    IndicateDeviceNotDisabled();
    return;
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->GetDeviceMode() == policy::DEVICE_MODE_PENDING) {
    // Ensure that the device mode is known before proceeding.
    connector->GetInstallAttributes()->ReadImmutableAttributes(
        base::Bind(&DeviceDisabledScreen::Show,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  if (connector->GetDeviceMode() != policy::DEVICE_MODE_NOT_SET) {
    // Skip the screen if the device is owned already. The device disabling
    // screen should only be shown during OOBE.
    IndicateDeviceNotDisabled();
    return;
  }

  showing_ = true;

  std::string message;
  g_browser_process->local_state()->GetDictionary(
      prefs::kServerBackedDeviceState)->GetString(
          policy::kDeviceStateDisabledMessage,
          &message);
  actor_->Show(message);
}

void DeviceDisabledScreen::Hide() {
  if (!showing_)
    return;
  showing_ = false;

  if (actor_)
    actor_->Hide();
}

std::string DeviceDisabledScreen::GetName() const {
  return WizardController::kDeviceDisabledScreenName;
}

void DeviceDisabledScreen::OnActorDestroyed(DeviceDisabledScreenActor* actor) {
  if (actor_ == actor)
    actor_ = nullptr;
}

void DeviceDisabledScreen::IndicateDeviceNotDisabled() {
  get_base_screen_delegate()->OnExit(BaseScreenDelegate::DEVICE_NOT_DISABLED);
}

}  // namespace chromeos
