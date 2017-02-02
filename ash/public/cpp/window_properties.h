// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
#define ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_

#include <stdint.h>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ui/base/class_property.h"

namespace aura {
template <typename T>
using WindowProperty = ui::ClassProperty<T>;
}

namespace ash {

// Shell-specific window property keys for use by ash and its clients.

// Alphabetical sort.

// If true (and the window is a panel), it's attached to its shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kPanelAttachedKey;

// A property key to store the type of a window's shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<int32_t>* const
    kShelfItemTypeKey;

// Alphabetical sort.

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
