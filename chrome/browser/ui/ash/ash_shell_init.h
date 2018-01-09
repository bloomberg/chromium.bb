// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_SHELL_INIT_H_
#define CHROME_BROWSER_UI_ASH_ASH_SHELL_INIT_H_

#include <memory>

#include "base/macros.h"

namespace ash {
class WindowManager;
}  // namespace ash

// Class responisble for initializing and destroying the Ash Shell for both
// CLASSIC and MUS modes.
class AshShellInit {
 public:
  AshShellInit();
  ~AshShellInit();

 private:
  // Only created when running in ash::Config::MUS.
  std::unique_ptr<ash::WindowManager> window_manager_;

  DISALLOW_COPY_AND_ASSIGN(AshShellInit);
};

#endif  // CHROME_BROWSER_UI_ASH_ASH_SHELL_INIT_H_
