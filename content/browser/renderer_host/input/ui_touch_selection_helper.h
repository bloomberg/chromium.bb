// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UI_TOUCH_SELECTION_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_UI_TOUCH_SELECTION_HELPER_H_

#include "ui/gfx/selection_bound.h"

namespace cc {
struct ViewportSelectionBound;
}

namespace content {

gfx::SelectionBound ConvertSelectionBound(
    const cc::ViewportSelectionBound& bound);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UI_TOUCH_SELECTION_HELPER_H_
