// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/launcher_test_api.h"

#include "ash/launcher/launcher.h"

namespace ash {
namespace test {

LauncherTestAPI::LauncherTestAPI(Launcher* launcher)
    : launcher_(launcher) {
}

LauncherTestAPI::~LauncherTestAPI() {
}

internal::LauncherView* LauncherTestAPI::launcher_view() {
  return launcher_->launcher_view_;
}

}  // namespace test
}  // namespace ash
