// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TABS_DOCK_INFO_ASH_H_
#define CHROME_BROWSER_UI_ASH_TABS_DOCK_INFO_ASH_H_

#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "ui/gfx/native_widget_types.h"

#include <set>

namespace chrome {
namespace ash {

DockInfo GetDockInfoAtPointAsh(
    const gfx::Point& screen_point, const std::set<gfx::NativeView>& ignore);

gfx::NativeView GetLocalProcessWindowAtPointAsh(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeView>& ignore);

}  // namespace ash
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_TABS_DOCK_INFO_ASH_H_
