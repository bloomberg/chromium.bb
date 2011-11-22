// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_SNAPSHOT_WINDOW_SNAPSHOT_H_
#define CHROME_BROWSER_UI_WINDOW_SNAPSHOT_WINDOW_SNAPSHOT_H_
#pragma once

#include <vector>

#include "ui/gfx/native_widget_types.h"

namespace gfx {
  class Rect;
}

namespace browser {

// Grabs a snapshot of the rectangle area |snapshot_bounds| with respect to the
// top left corner of the designated window and stores a PNG representation
// into a byte vector. Returns true if the operation is successful.
bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_WINDOW_SNAPSHOT_WINDOW_SNAPSHOT_H_
