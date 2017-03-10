// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_INTERFACES_SHELF_STRUCT_TRAITS_H_
#define ASH_PUBLIC_INTERFACES_SHELF_STRUCT_TRAITS_H_

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/shelf.mojom.h"

namespace mojo {

template <>
struct EnumTraits<ash::mojom::ShelfAction, ash::ShelfAction> {
  static ash::mojom::ShelfAction ToMojom(ash::ShelfAction input) {
    switch (input) {
      case ash::SHELF_ACTION_NONE:
        return ash::mojom::ShelfAction::NONE;
      case ash::SHELF_ACTION_NEW_WINDOW_CREATED:
        return ash::mojom::ShelfAction::WINDOW_CREATED;
      case ash::SHELF_ACTION_WINDOW_ACTIVATED:
        return ash::mojom::ShelfAction::WINDOW_ACTIVATED;
      case ash::SHELF_ACTION_WINDOW_MINIMIZED:
        return ash::mojom::ShelfAction::WINDOW_MINIMIZED;
      case ash::SHELF_ACTION_APP_LIST_SHOWN:
        return ash::mojom::ShelfAction::APP_LIST_SHOWN;
    }
    NOTREACHED();
    return ash::mojom::ShelfAction::NONE;
  }

  static bool FromMojom(ash::mojom::ShelfAction input, ash::ShelfAction* out) {
    switch (input) {
      case ash::mojom::ShelfAction::NONE:
        *out = ash::SHELF_ACTION_NONE;
        return true;
      case ash::mojom::ShelfAction::WINDOW_CREATED:
        *out = ash::SHELF_ACTION_NEW_WINDOW_CREATED;
        return true;
      case ash::mojom::ShelfAction::WINDOW_ACTIVATED:
        *out = ash::SHELF_ACTION_WINDOW_ACTIVATED;
        return true;
      case ash::mojom::ShelfAction::WINDOW_MINIMIZED:
        *out = ash::SHELF_ACTION_WINDOW_MINIMIZED;
        return true;
      case ash::mojom::ShelfAction::APP_LIST_SHOWN:
        *out = ash::SHELF_ACTION_APP_LIST_SHOWN;
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

template <>
struct EnumTraits<ash::mojom::ShelfLaunchSource, ash::ShelfLaunchSource> {
  static ash::mojom::ShelfLaunchSource ToMojom(ash::ShelfLaunchSource input) {
    switch (input) {
      case ash::LAUNCH_FROM_UNKNOWN:
        return ash::mojom::ShelfLaunchSource::UNKNOWN;
      case ash::LAUNCH_FROM_APP_LIST:
        return ash::mojom::ShelfLaunchSource::APP_LIST;
      case ash::LAUNCH_FROM_APP_LIST_SEARCH:
        return ash::mojom::ShelfLaunchSource::APP_LIST_SEARCH;
    }
    NOTREACHED();
    return ash::mojom::ShelfLaunchSource::UNKNOWN;
  }

  static bool FromMojom(ash::mojom::ShelfLaunchSource input,
                        ash::ShelfLaunchSource* out) {
    switch (input) {
      case ash::mojom::ShelfLaunchSource::UNKNOWN:
        *out = ash::LAUNCH_FROM_UNKNOWN;
        return true;
      case ash::mojom::ShelfLaunchSource::APP_LIST:
        *out = ash::LAUNCH_FROM_APP_LIST;
        return true;
      case ash::mojom::ShelfLaunchSource::APP_LIST_SEARCH:
        *out = ash::LAUNCH_FROM_APP_LIST_SEARCH;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // ASH_PUBLIC_INTERFACES_SHELF_STRUCT_TRAITS_H_
