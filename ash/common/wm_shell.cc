// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_shell.h"

namespace ash {

// static
WmShell* WmShell::instance_ = nullptr;

// static
void WmShell::Set(WmShell* instance) {
  instance_ = instance;
}

// static
WmShell* WmShell::Get() {
  return instance_;
}

}  // namespace ash
