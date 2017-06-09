// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_controller.h"

namespace ash {

ImeController::ImeController() = default;

ImeController::~ImeController() = default;

IMEInfo ImeController::GetCurrentIme() const {
  return IMEInfo();
}

std::vector<IMEPropertyInfo> ImeController::GetCurrentImeProperties() const {
  return std::vector<IMEPropertyInfo>();
}

std::vector<IMEInfo> ImeController::GetAvailableImes() const {
  return std::vector<IMEInfo>();
}

bool ImeController::IsImeManaged() const {
  return false;
}

}  // namespace ash
