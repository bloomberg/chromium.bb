// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_TEST_UTIL_H_

#include "ash/public/cpp/shelf_types.h"
#include "ui/events/types/event_type.h"

// Calls ShelfItemDelegate::ItemSelected for the item with the given |id|, using
// an event corresponding to the requested |event_type| and plumbs the requested
// |display_id| (invalid display id is mapped the primary display).
ash::ShelfAction SelectShelfItem(
    const ash::ShelfID& id,
    ui::EventType event_type,
    int64_t display_id,
    ash::ShelfLaunchSource source = ash::LAUNCH_FROM_UNKNOWN);

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_CONTROLLER_TEST_UTIL_H_
