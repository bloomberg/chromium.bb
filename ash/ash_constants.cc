// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_constants.h"

#include "ui/aura/window_property.h"

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ASH_EXPORT, bool)

namespace ash {

DEFINE_WINDOW_PROPERTY_KEY(bool, kConstrainedWindowKey, false);

const int kResizeAreaCornerSize = 16;
const int kResizeOutsideBoundsSize = 6;
const int kResizeOutsideBoundsScaleForTouch = 5;
const int kResizeInsideBoundsSize = 1;

}  // namespace ash
