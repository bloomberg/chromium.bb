// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_backdrop_delegate_impl.h"

namespace ash {

TabletModeBackdropDelegateImpl::TabletModeBackdropDelegateImpl() = default;

TabletModeBackdropDelegateImpl::~TabletModeBackdropDelegateImpl() = default;

bool TabletModeBackdropDelegateImpl::HasBackdrop(aura::Window* window) {
  return true;
}

}  // namespace ash
