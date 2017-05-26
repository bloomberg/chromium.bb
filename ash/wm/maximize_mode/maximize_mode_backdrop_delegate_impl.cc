// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_backdrop_delegate_impl.h"

namespace ash {

MaximizeModeBackdropDelegateImpl::MaximizeModeBackdropDelegateImpl() = default;

MaximizeModeBackdropDelegateImpl::~MaximizeModeBackdropDelegateImpl() = default;

bool MaximizeModeBackdropDelegateImpl::HasBackdrop(aura::Window* window) {
  return true;
}

}  // namespace ash
