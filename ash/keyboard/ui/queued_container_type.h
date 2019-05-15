// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_QUEUED_CONTAINER_TYPE_H_
#define ASH_KEYBOARD_UI_QUEUED_CONTAINER_TYPE_H_

#include "ash/public/interfaces/keyboard_controller_types.mojom.h"
#include "base/bind.h"
#include "base/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace keyboard {

class KeyboardController;

// Tracks a queued ContainerType change request. Couples a container type with a
// callback to invoke once the necessary animation and container changes are
// complete.
// The callback will be invoked once this object goes out of scope. Success
// is defined as the KeyboardController's current container behavior matching
// the same container type as the queued container type.
class QueuedContainerType {
 public:
  QueuedContainerType(KeyboardController* controller,
                      mojom::ContainerType container_type,
                      base::Optional<gfx::Rect> bounds,
                      base::OnceCallback<void(bool success)> callback);
  ~QueuedContainerType();
  mojom::ContainerType container_type() { return container_type_; }
  base::Optional<gfx::Rect> target_bounds() { return bounds_; }

 private:
  KeyboardController* controller_;
  mojom::ContainerType container_type_;
  base::Optional<gfx::Rect> bounds_;
  base::OnceCallback<void(bool success)> callback_;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_QUEUED_CONTAINER_TYPE_H_
