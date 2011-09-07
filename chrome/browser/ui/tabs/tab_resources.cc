// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

}  // namespace

// static
void TabResources::GetHitTestMask(int width, int height, gfx::Path* path) {
  DCHECK(path);

  SkScalar h = SkIntToScalar(height);
  SkScalar w = SkIntToScalar(width);

  path->moveTo(0, h);

  // Left end cap.
  path->lineTo(kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(kTabCapWidth, 0);

  // Connect to the right cap.
  path->lineTo(w - kTabCapWidth, 0);

  // Right end cap.
  path->lineTo(w - kTabCapWidth + kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(w - kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(w, h);

  // Close out the path.
  path->lineTo(0, h);
  path->close();
}
