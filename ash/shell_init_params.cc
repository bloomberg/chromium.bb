// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_init_params.h"

#include "ash/shell_delegate.h"

namespace ash {

#if defined(OS_WIN)
ShellInitParams::ShellInitParams() : delegate(NULL), remote_hwnd(NULL) {}
#else
ShellInitParams::ShellInitParams() : delegate(NULL) {}
#endif

ShellInitParams::~ShellInitParams() {}

}  // namespace ash
