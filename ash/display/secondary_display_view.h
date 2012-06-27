// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SECONDARY_DISPLAY_VIEW_H_
#define ASH_DISPLAY_SECONDARY_DISPLAY_VIEW_H_
#pragma once

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

// Creates the widget that hosts the static message displayed on the
// secondary display.
views::Widget* CreateSecondaryDisplayWidget(aura::Window* parent);

}  // namespace ash

#endif  // ASH_DISPLAY_SECONDARY_DISPLAY_VIEW_H_
