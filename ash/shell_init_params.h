// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_INIT_PARAMS_H_
#define ASH_SHELL_INIT_PARAMS_H_

#include "ash/ash_export.h"

namespace aura {
class WindowTreeHostMus;
}

namespace base {
class SequencedWorkerPool;
}

namespace ui {
class ContextFactory;
}

namespace ash {

class ShellDelegate;

struct ASH_EXPORT ShellInitParams {
  // Shell takes ownership of |wm_shell|, if null WmShellAura is created.
  // TODO(sky): temporary, will eventually go away.
  WmShell* wm_shell = nullptr;
  // Shell takes ownership of |primary_window_tree_host|. This is only used
  // by mash.
  aura::WindowTreeHostMus* primary_window_tree_host = nullptr;
  ShellDelegate* delegate = nullptr;
  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
  base::SequencedWorkerPool* blocking_pool = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELL_INIT_PARAMS_H_
