// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGH_CONTRAST_HIGH_CONTRAST_CONTROLLER_H_
#define ASH_HIGH_CONTRAST_HIGH_CONTRAST_CONTROLLER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace aura {
class RootWindow;
}

namespace ash {

class ASH_EXPORT HighContrastController {
 public:
  HighContrastController();

  ~HighContrastController() {}

  void SetEnabled(bool enabled);

 private:
  aura::RootWindow* root_window_;

  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(HighContrastController);
};

}  // namespace ash

#endif  // ASH_HIGH_CONTRAST_HIGH_CONTRAST_CONTROLLER_H_
