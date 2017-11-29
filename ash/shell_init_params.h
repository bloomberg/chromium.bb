// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_INIT_PARAMS_H_
#define ASH_SHELL_INIT_PARAMS_H_

#include <memory>

#include "ash/ash_export.h"

namespace ui {
class ContextFactory;
class ContextFactoryPrivate;
}

namespace ash {

class ShellDelegate;
class ShellPort;

struct ASH_EXPORT ShellInitParams {
  ShellInitParams();
  ShellInitParams(ShellInitParams&& other);
  ~ShellInitParams();

  std::unique_ptr<ShellPort> shell_port;
  std::unique_ptr<ShellDelegate> delegate;
  ui::ContextFactory* context_factory = nullptr;                 // Non-owning.
  ui::ContextFactoryPrivate* context_factory_private = nullptr;  // Non-owning.
};

}  // namespace ash

#endif  // ASH_SHELL_INIT_PARAMS_H_
