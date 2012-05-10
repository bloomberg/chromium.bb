// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SHELF_AUTO_HIDE_BEHAVIOR_H_
#define ASH_WM_SHELF_AUTO_HIDE_BEHAVIOR_H_
#pragma once

// TODO(sky): rename this file to shelf_types.

namespace ash {

enum ShelfAlignment {
  SHELF_ALIGNMENT_BOTTOM,
  SHELF_ALIGNMENT_LEFT,
  SHELF_ALIGNMENT_RIGHT,
};

enum ShelfAutoHideBehavior {
  // The default; maximized windows trigger an auto-hide.
  SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT,

  // Always auto-hide.
  SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,

  // Never auto-hide.
  SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
};

}  // namespace ash

#endif  // ASH_WM_SHELF_AUTO_HIDE_BEHAVIOR_H_
