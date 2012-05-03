// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_resources.h"

#include "base/logging.h"
#include "ui/gfx/path.h"

namespace {

// Hit mask constants.
const SkScalar kTabCapWidth = 15;
const SkScalar kTabTopCurveWidth = 4;
const SkScalar kTabBottomCurveWidth = 3;
#if defined(TOOLKIT_VIEWS)
// Windows and Ash have shadows in the left, right and top parts of the tab.
const SkScalar kTabInset = 6;
const SkScalar kTabTop = 2;
#else
// Linux GTK and Mac don't have the shadows.
const SkScalar kTabInset = 0;
const SkScalar kTabTop = 0;
#endif

}  // namespace

// static
void TabResources::GetHitTestMask(int width,
                                  int height,
                                  bool include_top_shadow,
                                  gfx::Path* path) {
  DCHECK(path);

  SkScalar left = kTabInset;
  SkScalar top = kTabTop;
  SkScalar right = SkIntToScalar(width) - kTabInset;
  SkScalar bottom = SkIntToScalar(height);

  // Start in the lower-left corner.
  path->moveTo(left, bottom);

  // Left end cap.
  path->lineTo(left + kTabBottomCurveWidth, bottom - kTabBottomCurveWidth);
  path->lineTo(left + kTabCapWidth - kTabTopCurveWidth,
               top + kTabTopCurveWidth);
  path->lineTo(left + kTabCapWidth, top);

  // Extend over the top shadow area if we have one and the caller wants it.
  if (kTabTop > 0 && include_top_shadow) {
    path->lineTo(left + kTabCapWidth, 0);
    path->lineTo(right - kTabCapWidth, 0);
  }

  // Connect to the right cap.
  path->lineTo(right - kTabCapWidth, top);

  // Right end cap.
  path->lineTo(right - kTabCapWidth + kTabTopCurveWidth,
               top + kTabTopCurveWidth);
  path->lineTo(right - kTabBottomCurveWidth, bottom - kTabBottomCurveWidth);
  path->lineTo(right, bottom);

  // Close out the path.
  path->lineTo(left, bottom);
  path->close();
}
