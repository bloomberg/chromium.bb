// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_controller.h"

#include "ash/public/interfaces/ime_info.mojom.h"

namespace ash {

ImeController::ImeController() = default;

ImeController::~ImeController() = default;

mojom::ImeInfo ImeController::GetCurrentIme() const {
  return mojom::ImeInfo();
}

std::vector<mojom::ImeInfo> ImeController::GetAvailableImes() const {
  return std::vector<mojom::ImeInfo>();
}

bool ImeController::IsImeManaged() const {
  return false;
}

std::vector<mojom::ImeMenuItem> ImeController::GetCurrentImeMenuItems() const {
  return std::vector<mojom::ImeMenuItem>();
}

}  // namespace ash
