// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/wm_globals.h"

namespace ash {
namespace wm {

// static
WmGlobals* WmGlobals::instance_ = nullptr;

// static
void WmGlobals::Set(WmGlobals* instance) {
  instance_ = instance;
}

// static
WmGlobals* WmGlobals::Get() {
  return instance_;
}

}  // namespace wm
}  // namespace ash
