// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/window_properties.h"

#include "ui/base/class_property.h"

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kRenderTitleAreaProperty, false);

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowIsJanky, false);

}  // namespace ash
