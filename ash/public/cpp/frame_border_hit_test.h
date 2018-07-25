// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FRAME_BORDER_HIT_TEST_H_
#define ASH_PUBLIC_CPP_FRAME_BORDER_HIT_TEST_H_

#include "ash/public/cpp/ash_public_export.h"

namespace gfx {
class Point;
}

namespace views {
class NonClientFrameView;
}  // namespace views

namespace ash {

// Returns the HitTestCompat for the specified point.
ASH_PUBLIC_EXPORT int FrameBorderNonClientHitTest(
    views::NonClientFrameView* view,
    const gfx::Point& point_in_widget);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FRAME_BORDER_HIT_TEST_H_
