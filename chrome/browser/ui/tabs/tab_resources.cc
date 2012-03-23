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
#if defined(USE_ASH)
// Ash has shadows in the left and right part of the tab.
const SkScalar kTabInset = 6;
#else
const SkScalar kTabInset = 0;
#endif

}  // namespace

// static
void TabResources::GetHitTestMask(int width, int height, gfx::Path* path) {
  DCHECK(path);

  SkScalar h = SkIntToScalar(height);
  SkScalar w = SkIntToScalar(width);

  SkScalar left = kTabInset;
  path->moveTo(left, h);

  // Left end cap.
  path->lineTo(left + kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(left + kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(left + kTabCapWidth, 0);

  // Connect to the right cap.
  SkScalar right = w - kTabInset;
  path->lineTo(right - kTabCapWidth, 0);

  // Right end cap.
  path->lineTo(right - kTabCapWidth + kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(right - kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(right, h);

  // Close out the path.
  path->lineTo(left, h);
  path->close();
}
