// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_shelf_mus.h"

#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"

namespace ash {
namespace mus {

WmShelfMus::WmShelfMus() {}

WmShelfMus::~WmShelfMus() {}

void WmShelfMus::WillDeleteShelfLayoutManager() {
  ShutdownShelfWidget();
  WmShelf::WillDeleteShelfLayoutManager();
}

}  // namespace mus
}  // namespace ash
