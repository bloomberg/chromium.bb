// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/wm_shell_test_api.h"

#include <utility>

#include "ash/common/wm_shell.h"
#include "ash/public/interfaces/new_window.mojom.h"
#include "ash/system/tray/system_tray_delegate.h"

namespace ash {

WmShellTestApi::WmShellTestApi() {}

WmShellTestApi::~WmShellTestApi() {}

void WmShellTestApi::SetSystemTrayDelegate(
    std::unique_ptr<SystemTrayDelegate> delegate) {
  WmShell::Get()->SetSystemTrayDelegate(std::move(delegate));
}

}  // namespace ash
