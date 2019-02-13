// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_CONTROLLER_H_
#define ASH_WM_DESKS_DESKS_CONTROLLER_H_

#include "base/macros.h"

namespace ash {

// Defines a controller for creating, destroying and managing virtual desks and
// their windows.
class DesksController {
 public:
  DesksController() = default;
  ~DesksController() = default;

  // Convenience method for returning the DesksController instance. The actual
  // instance is created and owned by Shell.
  static DesksController* Get();

  // Returns true if we haven't reached the maximum allowed number of desks.
  bool CanCreateDesks() const;

  void NewDesk();

 private:
  DISALLOW_COPY_AND_ASSIGN(DesksController);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_CONTROLLER_H_
