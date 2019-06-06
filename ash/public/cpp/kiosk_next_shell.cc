// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/kiosk_next_shell.h"

#include "base/logging.h"

namespace ash {

namespace {
KioskNextShellController* g_instance = nullptr;
}

// static
KioskNextShellController* KioskNextShellController::Get() {
  return g_instance;
}

KioskNextShellController::KioskNextShellController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

KioskNextShellController::~KioskNextShellController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
