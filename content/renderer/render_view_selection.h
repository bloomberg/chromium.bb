// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_SELECTION_H_
#define CONTENT_RENDERER_RENDER_VIEW_SELECTION_H_
#pragma once

#include <string>
#include "ui/base/range/range.h"
#include "ui/gfx/point.h"

// Please do not use this class. This is to be deleted.
// A data class to represent selection on a webpage.
// TODO(varunjain): delete this class.
class RenderViewSelection {
 public:
  RenderViewSelection();
  RenderViewSelection(const std::string& text,
                      const ui::Range range,
                      const gfx::Point start,
                      const gfx::Point end);

  virtual ~RenderViewSelection();

  bool Equals(const RenderViewSelection& that) const;

 private:
  std::string text_;
  ui::Range range_;
  gfx::Point start_;
  gfx::Point end_;
};

#endif  // CONTENT_RENDERER_RENDER_VIEW_SELECTION_H_
