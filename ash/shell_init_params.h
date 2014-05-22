// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_INIT_PARAMS_H_
#define ASH_SHELL_INIT_PARAMS_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "ash/ash_export.h"

namespace ui {
class ContextFactory;
}

namespace ash {

class ShellDelegate;

struct ASH_EXPORT ShellInitParams {
  ShellInitParams();
  ~ShellInitParams();

  ShellDelegate* delegate;

  ui::ContextFactory* context_factory;

#if defined(OS_WIN)
  HWND remote_hwnd;
#endif
};

}  // namespace ash

#endif  // ASH_SHELL_INIT_PARAMS_H_
