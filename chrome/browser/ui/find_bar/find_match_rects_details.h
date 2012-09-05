// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_MATCH_RECTS_DETAILS_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_MATCH_RECTS_DETAILS_H_

#include <vector>

#include "base/basictypes.h"
#include "ui/gfx/rect_f.h"

// Holds the result details of a RequestFindMatchRects reply.
class FindMatchRectsDetails {
 public:
  FindMatchRectsDetails();

  FindMatchRectsDetails(int version,
                        const std::vector<gfx::RectF>& rects,
                        const gfx::RectF& active_rect);

  ~FindMatchRectsDetails();

  int version() const { return version_; }

  const std::vector<gfx::RectF>& rects() const { return rects_; }

  const gfx::RectF& active_rect() const { return active_rect_; }

 private:
  // The version of the rects in this structure.
  int version_;

  // The rects of the find matches in find-in-page coordinates.
  std::vector<gfx::RectF> rects_;

  // The rect of the active find match in find-in-page coordinates.
  gfx::RectF active_rect_;
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_MATCH_RECTS_DETAILS_H_
