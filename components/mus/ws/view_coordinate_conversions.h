// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_VIEW_COORDINATE_CONVERSIONS_H_
#define COMPONENTS_MUS_WS_VIEW_COORDINATE_CONVERSIONS_H_

namespace gfx {
class Point;
class PointF;
class Rect;
}

namespace mus {

class ServerView;

// Converts |point| from the coordinates of |from| to the coordinates of |to|.
// |from| and |to| must be an ancestors or descendants of each other.
gfx::Point ConvertPointBetweenViews(const ServerView* from,
                                    const ServerView* to,
                                    const gfx::Point& point);
gfx::PointF ConvertPointFBetweenViews(const ServerView* from,
                                      const ServerView* to,
                                      const gfx::PointF& point);

// Converts |rect| from the coordinates of |from| to the coordinates of |to|.
// |from| and |to| must be an ancestors or descendants of each other.
gfx::Rect ConvertRectBetweenViews(const ServerView* from,
                                  const ServerView* to,
                                  const gfx::Rect& rect);

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_VIEW_COORDINATE_CONVERSIONS_H_
