// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_properties.h"

#include "ash/public/interfaces/window_pin_type.mojom.h"

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_PUBLIC_EXPORT,
                                        ash::mojom::WindowPinType)

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPanelAttachedKey, true);
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kShelfItemTypeKey, TYPE_UNDEFINED);
DEFINE_UI_CLASS_PROPERTY_KEY(ash::mojom::WindowPinType,
                             kWindowPinTypeKey,
                             ash::mojom::WindowPinType::NONE);

}  // namespace ash
