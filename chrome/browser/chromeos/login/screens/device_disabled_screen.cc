// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

DeviceDisabledScreen::DeviceDisabledScreen(ScreenObserver* observer,
                                           DeviceDisabledScreenActor* actor)
    : BaseScreen(observer),
      actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

DeviceDisabledScreen::~DeviceDisabledScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void DeviceDisabledScreen::PrepareToShow() {
}

void DeviceDisabledScreen::Show() {
  if (actor_)
    actor_->Show();
}

void DeviceDisabledScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string DeviceDisabledScreen::GetName() const {
  return WizardController::kDeviceDisabledScreenName;
}

void DeviceDisabledScreen::OnActorDestroyed(DeviceDisabledScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
