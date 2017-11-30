// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/test_accessibility_controller_client.h"

namespace ash {

constexpr base::TimeDelta
    TestAccessibilityControllerClient::kShutdownSoundDuration;

TestAccessibilityControllerClient::TestAccessibilityControllerClient()
    : binding_(this) {}

TestAccessibilityControllerClient::~TestAccessibilityControllerClient() =
    default;

mojom::AccessibilityControllerClientPtr
TestAccessibilityControllerClient::CreateInterfacePtrAndBind() {
  mojom::AccessibilityControllerClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TestAccessibilityControllerClient::TriggerAccessibilityAlert(
    mojom::AccessibilityAlert alert) {
  last_a11y_alert_ = alert;
}

void TestAccessibilityControllerClient::PlayEarcon(int32_t sound_key) {
  sound_key_ = sound_key;
}

void TestAccessibilityControllerClient::PlayShutdownSound(
    PlayShutdownSoundCallback callback) {
  std::move(callback).Run(kShutdownSoundDuration);
}

int32_t TestAccessibilityControllerClient::GetPlayedEarconAndReset() {
  int32_t tmp = sound_key_;
  sound_key_ = -1;
  return tmp;
}

}  // namespace ash
