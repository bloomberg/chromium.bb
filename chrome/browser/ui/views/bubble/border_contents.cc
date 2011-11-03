// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/border_contents.h"

#include "chrome/browser/ui/window_sizer.h"

static const int kTopMargin = 6;
static const int kLeftMargin = 6;
static const int kBottomMargin = 9;
static const int kRightMargin = 6;

BorderContents::BorderContents()
    : BorderContentsView(kTopMargin,
                         kLeftMargin,
                         kBottomMargin,
                         kRightMargin) {}

gfx::Rect BorderContents::GetMonitorBounds(const gfx::Rect& rect) {
  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  return monitor_provider->GetMonitorWorkAreaMatching(rect);
}
