// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_WM_SHELF_UTIL_H_
#define ASH_COMMON_SHELF_WM_SHELF_UTIL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"

namespace ash {

// Returns true if the shelf |alignment| is horizontal.
// TODO(jamescook): Remove this in favor of WmShelf::IsHorizontalAlignment().
ASH_EXPORT bool IsHorizontalAlignment(ShelfAlignment alignment);

}  // namespace ash

#endif  // ASH_COMMON_SHELF_WM_SHELF_UTIL_H_
