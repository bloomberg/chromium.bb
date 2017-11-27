// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
#define ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_

#include <stdint.h>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/class_property.h"

namespace aura {
template <typename T>
using WindowProperty = ui::ClassProperty<T>;
}

namespace ash {

namespace mojom {
enum class WindowPinType;
enum class WindowStateType;
}

// Shell-specific window property keys for use by ash and its clients.

// Alphabetical sort.

// If true, will send system keys to the window for dispatch.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kCanConsumeSystemKeysKey;

// Whether the shelf should be hidden when this window is put into fullscreen.
// Exposed because some windows want to explicitly opt-out of this.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kHideShelfWhenFullscreenKey;

// If true (and the window is a panel), it's attached to its shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kPanelAttachedKey;

// A property key to store the id for a window's shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<std::string*>* const
    kShelfIDKey;

// A property key to store the type of a window's shelf item.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<int32_t>* const
    kShelfItemTypeKey;

// A property key to indicate whether we should hide this window in overview
// mode and Alt + Tab.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kShowInOverviewKey;

// A property key to store ash::WindowPinType for a window.
// When setting this property to PINNED or TRUSTED_PINNED, the window manager
// will try to fullscreen the window and pin it on the top of the screen. If the
// window manager failed to do it, the property will be restored to NONE. When
// setting this property to NONE, the window manager will restore the window.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<mojom::WindowPinType>* const
    kWindowPinTypeKey;

// A property key to indicate whether ash should perform auto management of
// window positions; when you open a second browser, ash will move the two to
// minimize overlap.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<bool>* const
    kWindowPositionManagedTypeKey;

// A property key to indicate ash's extended window state.
ASH_PUBLIC_EXPORT extern const aura::WindowProperty<
    mojom::WindowStateType>* const kWindowStateTypeKey;

// Alphabetical sort.

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_PROPERTIES_H_
