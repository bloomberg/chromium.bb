// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DOCK_DOCK_TYPES_H_
#define ASH_WM_DOCK_DOCK_TYPES_H_

namespace ash {

namespace internal {

// Possible values of which side of the screen the windows are docked at.
enum DockedAlignment {
  DOCKED_ALIGNMENT_NONE,
  DOCKED_ALIGNMENT_LEFT,
  DOCKED_ALIGNMENT_RIGHT,
  DOCKED_ALIGNMENT_ANY,
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_DOCK_DOCK_TYPES_H_
