// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"

namespace chromeos {

DeviceDisabledScreen::DeviceDisabledScreen(
    BaseScreenDelegate* base_screen_delegate,
    DeviceDisabledScreenActor* actor)
    : BaseScreen(base_screen_delegate),
      showing_(false),
      actor_(actor) {
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

  showing_ = true;
  actor_->Show(g_browser_process->platform_part()->device_disabling_manager()->
                  disabled_message());
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

}  // namespace chromeos
