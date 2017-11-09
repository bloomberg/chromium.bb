// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_properties.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/public/interfaces/window_state_type.mojom.h"

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                        ash::mojom::WindowPinType)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                        ash::mojom::WindowStateType)

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCanConsumeSystemKeysKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPanelAttachedKey, true);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kShelfIDKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kShelfItemTypeKey, TYPE_UNDEFINED);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kShowInOverviewKey, true);
DEFINE_UI_CLASS_PROPERTY_KEY(mojom::WindowPinType,
                             kWindowPinTypeKey,
                             mojom::WindowPinType::NONE);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowPositionManagedTypeKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(mojom::WindowStateType,
                             kWindowStateTypeKey,
                             mojom::WindowStateType::DEFAULT);

}  // namespace ash
