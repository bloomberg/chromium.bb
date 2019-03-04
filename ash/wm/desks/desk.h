// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_H_
#define ASH_WM_DESKS_DESK_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// Represents a virtual desk, tracking the windows that belong to this desk.
// TODO(afakhry): Fill in this class.
class ASH_EXPORT Desk {
 public:
  Desk();
  ~Desk();

 private:
  DISALLOW_COPY_AND_ASSIGN(Desk);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_H_
