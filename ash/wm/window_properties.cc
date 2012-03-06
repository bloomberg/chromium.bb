// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ash/wm/shadow_types.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::ShadowType);

namespace ash {
namespace internal {
DEFINE_WINDOW_PROPERTY_KEY(ShadowType, kShadowTypeKey, SHADOW_TYPE_NONE);
}  // namespace internal
}  // namespace ash
