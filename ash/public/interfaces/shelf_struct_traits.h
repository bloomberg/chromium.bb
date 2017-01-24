// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_INTERFACES_SHELF_STRUCT_TRAITS_H_
#define ASH_PUBLIC_INTERFACES_SHELF_STRUCT_TRAITS_H_

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/shelf.mojom.h"

namespace mojo {

template <>
struct EnumTraits<ash::mojom::ShelfAlignment, ash::ShelfAlignment> {
  static ash::mojom::ShelfAlignment ToMojom(ash::ShelfAlignment input) {
    switch (input) {
      case ash::SHELF_ALIGNMENT_BOTTOM:
        return ash::mojom::ShelfAlignment::BOTTOM;
      case ash::SHELF_ALIGNMENT_LEFT:
        return ash::mojom::ShelfAlignment::LEFT;
      case ash::SHELF_ALIGNMENT_RIGHT:
        return ash::mojom::ShelfAlignment::RIGHT;
      case ash::SHELF_ALIGNMENT_BOTTOM_LOCKED:
        return ash::mojom::ShelfAlignment::BOTTOM_LOCKED;
    }
    NOTREACHED();
    return ash::mojom::ShelfAlignment::BOTTOM;
  }

  static bool FromMojom(ash::mojom::ShelfAlignment input,
                        ash::ShelfAlignment* out) {
    switch (input) {
      case ash::mojom::ShelfAlignment::BOTTOM:
        *out = ash::SHELF_ALIGNMENT_BOTTOM;
        return true;
      case ash::mojom::ShelfAlignment::LEFT:
        *out = ash::SHELF_ALIGNMENT_LEFT;
        return true;
      case ash::mojom::ShelfAlignment::RIGHT:
        *out = ash::SHELF_ALIGNMENT_RIGHT;
        return true;
      case ash::mojom::ShelfAlignment::BOTTOM_LOCKED:
        *out = ash::SHELF_ALIGNMENT_BOTTOM_LOCKED;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<ash::mojom::ShelfAutoHideBehavior,
                  ash::ShelfAutoHideBehavior> {
  static ash::mojom::ShelfAutoHideBehavior ToMojom(
      ash::ShelfAutoHideBehavior input) {
    switch (input) {
      case ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
        return ash::mojom::ShelfAutoHideBehavior::ALWAYS;
      case ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
        return ash::mojom::ShelfAutoHideBehavior::NEVER;
      case ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
        return ash::mojom::ShelfAutoHideBehavior::HIDDEN;
    }
    NOTREACHED();
    return ash::mojom::ShelfAutoHideBehavior::NEVER;
  }

  static bool FromMojom(ash::mojom::ShelfAutoHideBehavior input,
                        ash::ShelfAutoHideBehavior* out) {
    switch (input) {
      case ash::mojom::ShelfAutoHideBehavior::ALWAYS:
        *out = ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
        return true;
      case ash::mojom::ShelfAutoHideBehavior::NEVER:
        *out = ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
        return true;
      case ash::mojom::ShelfAutoHideBehavior::HIDDEN:
        *out = ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // ASH_PUBLIC_INTERFACES_SHELF_STRUCT_TRAITS_H_
