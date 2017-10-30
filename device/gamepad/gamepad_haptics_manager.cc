// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_haptics_manager.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

GamepadHapticsManager::GamepadHapticsManager() = default;

GamepadHapticsManager::~GamepadHapticsManager() = default;

// static
void GamepadHapticsManager::Create(
    mojom::GamepadHapticsManagerRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<GamepadHapticsManager>(),
                          std::move(request));
}

void GamepadHapticsManager::PlayVibrationEffectOnce(
    int pad_index,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    PlayVibrationEffectOnceCallback callback) {
  NOTIMPLEMENTED();
}

void GamepadHapticsManager::ResetVibrationActuator(
    int pad_index,
    ResetVibrationActuatorCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace device
