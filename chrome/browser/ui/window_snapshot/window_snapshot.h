// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_SNAPSHOT_WINDOW_SNAPSHOT_H_
#define CHROME_BROWSER_UI_WINDOW_SNAPSHOT_WINDOW_SNAPSHOT_H_

#include <vector>

#include "ui/gfx/native_widget_types.h"

class PrefRegistrySimple;

namespace gfx {
class Rect;
}

namespace chrome {

void RegisterScreenshotPrefs(PrefRegistrySimple* registry);

// Grabs a snapshot of the rectangle area |snapshot_bounds| with respect to the
// top left corner of the designated window and stores a PNG representation
// into a byte vector. On Windows, |window| may be NULL to grab a snapshot of
// the primary monitor. This takes into account calling user context (ie. checks
// policy settings if taking screenshots is allowed), and is intended to be used
// by browser code. If you need to take a screenshot for debugging purposes,
// consider using ui::GrabWindowSnapshot.
// Returns true if the operation is successful (ie. permitted).
bool GrabWindowSnapshotForUser(
    gfx::NativeWindow window,
    std::vector<unsigned char>* png_representation,
    const gfx::Rect& snapshot_bounds);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_WINDOW_SNAPSHOT_WINDOW_SNAPSHOT_H_
