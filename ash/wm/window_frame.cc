// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_frame.h"

#include "ui/aura/window_property.h"

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ASH_EXPORT, ash::WindowFrame*);

namespace ash {
DEFINE_WINDOW_PROPERTY_KEY(WindowFrame*, kWindowFrameKey, NULL);
}  // namespace ash
