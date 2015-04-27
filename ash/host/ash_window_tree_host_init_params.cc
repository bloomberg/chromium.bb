// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_init_params.h"

namespace ash {

AshWindowTreeHostInitParams::AshWindowTreeHostInitParams()
    : offscreen(false)
#if defined(OS_WIN)
      ,
      remote_hwnd(NULL)
#endif
{
}

AshWindowTreeHostInitParams::~AshWindowTreeHostInitParams() {
}

}  // namespace ash
