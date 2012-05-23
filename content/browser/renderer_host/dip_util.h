// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DIP_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_DIP_UTIL_H_
#pragma once

#include "content/common/content_export.h"

namespace gfx {
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace content {
class RenderWidgetHostView;

// Returns scale factor used to convert from DIP to pixel coordinate systems.
// Returns 1.0 if DIP is not enabled.
CONTENT_EXPORT float GetDIPScaleFactor(const RenderWidgetHostView* view);

// Utility functions that convert point/size/rect between DIP and pixel
// coordinate system.
CONTENT_EXPORT gfx::Point ConvertPointToDIP(const RenderWidgetHostView* view,
                                            const gfx::Point& point_in_pixel);
CONTENT_EXPORT gfx::Size ConvertSizeToDIP(const RenderWidgetHostView* view,
                                          const gfx::Size& size_in_pixel);
CONTENT_EXPORT gfx::Rect ConvertRectToDIP(const RenderWidgetHostView* view,
                                          const gfx::Rect& rect_in_pixel);
CONTENT_EXPORT gfx::Point ConvertPointToPixel(const RenderWidgetHostView* view,
                                              const gfx::Point& point_in_dip);
CONTENT_EXPORT gfx::Size ConvertSizeToPixel(const RenderWidgetHostView* view,
                                            const gfx::Size& size_in_dip);
CONTENT_EXPORT gfx::Rect ConvertRectToPixel(const RenderWidgetHostView* view,
                                            const gfx::Rect& rect_in_dip);
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DIP_UTIL_H_
