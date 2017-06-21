// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_INIT_PARAMS_H_
#define ASH_SHELL_INIT_PARAMS_H_

#include "ash/ash_export.h"

namespace ui {
class ContextFactory;
class ContextFactoryPrivate;
}

namespace ash {

class ShellDelegate;
class ShellPort;

struct ASH_EXPORT ShellInitParams {
  // Shell takes ownership of |shell_port|, if null ShellPortClassic is created.
  ShellPort* shell_port = nullptr;
  ShellDelegate* delegate = nullptr;
  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
};

}  // namespace ash

#endif  // ASH_SHELL_INIT_PARAMS_H_
