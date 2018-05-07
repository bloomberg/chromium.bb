// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_properties.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                       ash::mojom::WindowPinType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                       ash::mojom::WindowStateType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                       ash::BackdropWindowMode)

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(BackdropWindowMode,
                             kBackdropWindowMode,
                             BackdropWindowMode::kAuto);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCanConsumeSystemKeysKey, false);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::ImageSkia,
                                   kFrameImageActiveKey,
                                   nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideShelfWhenFullscreenKey, true);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPanelAttachedKey, true);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kRestoreBoundsOverrideKey,
                                   nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(mojom::WindowStateType,
                             kRestoreWindowStateTypeOverrideKey,
                             mojom::WindowStateType::DEFAULT);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kShelfIDKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kShelfItemTypeKey, TYPE_UNDEFINED);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kShowInOverviewKey, true);
DEFINE_UI_CLASS_PROPERTY_KEY(SkColor,
                             kFrameActiveColorKey,
                             SK_ColorTRANSPARENT);
DEFINE_UI_CLASS_PROPERTY_KEY(SkColor,
                             kFrameInactiveColorKey,
                             SK_ColorTRANSPARENT);
DEFINE_UI_CLASS_PROPERTY_KEY(mojom::WindowPinType,
                             kWindowPinTypeKey,
                             mojom::WindowPinType::NONE);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowPositionManagedTypeKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(mojom::WindowStateType,
                             kWindowStateTypeKey,
                             mojom::WindowStateType::DEFAULT);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowTitleShownKey, true);

}  // namespace ash
