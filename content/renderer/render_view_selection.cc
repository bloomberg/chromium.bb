// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_selection.h"

RenderViewSelection::RenderViewSelection() {
}

RenderViewSelection::RenderViewSelection(const std::string& text,
                                         const ui::Range range,
                                         const gfx::Point start,
                                         const gfx::Point end)
    : text_(text),
      range_(range),
      start_(start),
      end_(end) {
}

RenderViewSelection::~RenderViewSelection() {
}

bool RenderViewSelection::Equals(const RenderViewSelection& that) const {
  return text_ == that.text_ &&
         range_ == that.range_ &&
         start_ == that.start_ &&
         end_ == that.end_;
}
