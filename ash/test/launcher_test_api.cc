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

internal::ShelfView* LauncherTestAPI::shelf_view() {
  return launcher_->shelf_view_;
}

void LauncherTestAPI::SetShelfDelegate(ShelfDelegate* delegate) {
  launcher_->delegate_ = delegate;
}

}  // namespace test
}  // namespace ash
