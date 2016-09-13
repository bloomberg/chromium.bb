// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_INIT_PARAMS_H_
#define ASH_SHELL_INIT_PARAMS_H_

#include "ash/ash_export.h"

namespace base {
class SequencedWorkerPool;
}

namespace ui {
class ContextFactory;
}

namespace ash {

class ShellDelegate;

struct ASH_EXPORT ShellInitParams {
  ShellDelegate* delegate = nullptr;
  ui::ContextFactory* context_factory = nullptr;
  base::SequencedWorkerPool* blocking_pool = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELL_INIT_PARAMS_H_
