// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_INIT_H_
#define CHROME_BROWSER_UI_ASH_ASH_INIT_H_

#include <memory>

#include "base/macros.h"

namespace ash {
namespace mus {
class WindowManager;
}
}  // namespace ash

// Creates and owns ash.
// TODO(jamescook): Fold this into ChromeBrowserMainExtraPartsAsh.
class AshInit {
 public:
  AshInit();
  ~AshInit();

 private:
  // Only created when running in ash::Config::MUS.
  std::unique_ptr<ash::mus::WindowManager> window_manager_;

  // NOTE: Please add new code to ChromeBrowserMainExtraPartsAsh.

  DISALLOW_COPY_AND_ASSIGN(AshInit);
};

#endif  // CHROME_BROWSER_UI_ASH_ASH_INIT_H_
