// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_INTEGRATION_TEST_API_STRUCT_MOJOM_TRAITS_H_
#define ASH_PUBLIC_CPP_SHELF_INTEGRATION_TEST_API_STRUCT_MOJOM_TRAITS_H_

#include "ash/public/interfaces/shelf_integration_test_api.mojom-shared.h"

namespace mojo {
template <>
struct EnumTraits<ash::mojom::ShelfAutoHideBehavior,
                  ash::ShelfAutoHideBehavior> {
  static ash::mojom::ShelfAutoHideBehavior ToMojom(
      ash::ShelfAutoHideBehavior input) {
    switch (input) {
      case ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
        return ash::mojom::ShelfAutoHideBehavior::
            SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
      case ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
        return ash::mojom::ShelfAutoHideBehavior::
            SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
      case ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
        return ash::mojom::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_ALWAYS_HIDDEN;
    }
    NOTREACHED();
    return ash::mojom::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  }

  static bool FromMojom(ash::mojom::ShelfAutoHideBehavior input,
                        ash::ShelfAutoHideBehavior* out) {
    switch (input) {
      case ash::mojom::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
        *out = ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
        return true;
      case ash::mojom::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
        *out = ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
        return true;
      case ash::mojom::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
        *out = ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<ash::mojom::ShelfAlignment, ash::ShelfAlignment> {
  static ash::mojom::ShelfAlignment ToMojom(ash::ShelfAlignment input) {
    switch (input) {
      case ash::SHELF_ALIGNMENT_BOTTOM:
        return ash::mojom::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM;
      case ash::SHELF_ALIGNMENT_LEFT:
        return ash::mojom::ShelfAlignment::SHELF_ALIGNMENT_LEFT;
      case ash::SHELF_ALIGNMENT_RIGHT:
        return ash::mojom::ShelfAlignment::SHELF_ALIGNMENT_RIGHT;
      case ash::SHELF_ALIGNMENT_BOTTOM_LOCKED:
        return ash::mojom::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM_LOCKED;
    }
    NOTREACHED();
    return ash::mojom::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM;
  }

  static bool FromMojom(ash::mojom::ShelfAlignment input,
                        ash::ShelfAlignment* out) {
    switch (input) {
      case ash::mojom::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM:
        *out = ash::SHELF_ALIGNMENT_BOTTOM;
        return true;
      case ash::mojom::ShelfAlignment::SHELF_ALIGNMENT_LEFT:
        *out = ash::SHELF_ALIGNMENT_LEFT;
        return true;
      case ash::mojom::ShelfAlignment::SHELF_ALIGNMENT_RIGHT:
        *out = ash::SHELF_ALIGNMENT_RIGHT;
        return true;
      case ash::mojom::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM_LOCKED:
        *out = ash::SHELF_ALIGNMENT_BOTTOM_LOCKED;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // ASH_PUBLIC_CPP_SHELF_INTEGRATION_TEST_API_STRUCT_MOJOM_TRAITS_H_
