// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_init_params.h"

#include "ash/shell_delegate.h"

namespace ash {

ShellInitParams::ShellInitParams()
    : delegate(NULL),
      context_factory(NULL)
#if defined(OS_WIN)
      , remote_hwnd(NULL)
#endif
      {}

ShellInitParams::~ShellInitParams() {}

}  // namespace ash
