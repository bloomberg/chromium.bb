// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"

#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

HIDDetectionScreen::HIDDetectionScreen(
    ScreenObserver* observer, HIDDetectionScreenActor* actor)
    : WizardScreen(observer), actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

HIDDetectionScreen::~HIDDetectionScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void HIDDetectionScreen::PrepareToShow() {
}

void HIDDetectionScreen::Show() {
  if (actor_)
    actor_->Show();
}

void HIDDetectionScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string HIDDetectionScreen::GetName() const {
  return WizardController::kHIDDetectionScreenName;
}

void HIDDetectionScreen::OnExit() {
  get_screen_observer()->OnExit(ScreenObserver::HID_DETECTION_COMPLETED);
}

void HIDDetectionScreen::OnActorDestroyed(HIDDetectionScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
