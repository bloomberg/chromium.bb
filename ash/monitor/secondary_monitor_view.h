// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MONITOR_SECONDARY_MONITOR_VIEW_H_
#define ASH_MONITOR_SECONDARY_MONITOR_VIEW_H_
#pragma once

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

// Creates the widget that hosts the static message displayed on the
// secondary monitor.
views::Widget* CreateSecondaryMonitorWidget(aura::Window* parent);

}  // namespace ash

#endif  // ASH_MONITOR_SECONDARY_MONITOR_VIEW_H_
