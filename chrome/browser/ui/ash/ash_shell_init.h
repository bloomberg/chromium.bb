// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_SHELL_INIT_H_
#define CHROME_BROWSER_UI_ASH_ASH_SHELL_INIT_H_

#include <memory>

#include "base/macros.h"

class PrefRegistrySimple;

namespace ash {
class WindowManager;
}  // namespace ash

// Class responsible for initializing and destroying the Ash Shell for both
// CLASSIC and MUS modes.
class AshShellInit {
 public:
  AshShellInit();
  ~AshShellInit();

  // Registers ash display prefs so that they can be provided synchronously
  // to Shell from the local state (which is loaded asynchronously in Shell).
  // |registry| should be the PrefRegistry for local state prefs.
  // TODO(stevenjb): Improve how we do this. https://crbug.com/678949.
  static void RegisterDisplayPrefs(PrefRegistrySimple* registry);

 private:
  // Only created when running in ash::Config::MUS.
  std::unique_ptr<ash::WindowManager> window_manager_;

  DISALLOW_COPY_AND_ASSIGN(AshShellInit);
};

#endif  // CHROME_BROWSER_UI_ASH_ASH_SHELL_INIT_H_
