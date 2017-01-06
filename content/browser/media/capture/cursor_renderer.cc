// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/cursor_renderer.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "skia/ext/image_operations.h"

namespace content {

namespace {

inline int clip_byte(int x) {
  return std::max(0, std::min(x, 255));
}

inline int alpha_blend(int alpha, int src, int dst) {
  return (src * alpha + dst * (255 - alpha)) / 255;
}

}  // namespace

CursorRenderer::CursorRenderer(gfx::NativeView view,
                               CursorDisplaySetting cursor_display_setting)
    : captured_view_(view),
      cursor_display_setting_(cursor_display_setting),
      tick_clock_(&default_tick_clock_),
      weak_factory_(this) {
  Clear();
}

CursorRenderer::~CursorRenderer() {}

base::WeakPtr<CursorRenderer> CursorRenderer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void CursorRenderer::Clear() {
  last_x_scale_ = 0.0f;
  last_y_scale_ = 0.0f;
  last_cursor_ = gfx::NativeCursor();
  last_cursor_hot_point_ = gfx::Point();
  scaled_cursor_bitmap_.reset();
  last_mouse_position_x_ = 0;
  last_mouse_position_y_ = 0;
  last_mouse_movement_timestamp_ = base::TimeTicks::Now();
  if (cursor_display_setting_ == kCursorEnabledOnMouseMovement) {
    cursor_displayed_ = false;
  } else {
    cursor_displayed_ = true;
  }
}

bool CursorRenderer::SnapshotCursorState(const gfx::Rect& region_in_frame) {
  if (!captured_view_) {
    DVLOG(2) << "Skipping update with no view being tracked";
    return false;
  }

  // If we are sharing the root view, or namely the whole screen, we will
  // render the mouse cursor. For ordinary view, we only render the mouse
  // cursor when the view is active.
  if (!IsCapturedViewActive()) {
    // Return early if the target view is not active.
    DVLOG(2) << "Skipping update on an inactive view";
    Clear();
    return false;
  }

  gfx::Size view_size = GetCapturedViewSize();
  if (view_size.IsEmpty()) {
    DVLOG(2) << "Skipping update on an empty view";
    Clear();
    return false;
  }

  gfx::Point cursor_position = GetCursorPositionInView();
  if (!gfx::Rect(view_size).Contains(cursor_position)) {
    DVLOG(2) << "Skipping update with cursor outside the view";
    Clear();
    return false;
  }

  if (cursor_display_setting_ == kCursorEnabledOnMouseMovement) {
    if (cursor_displayed_) {
      // Stop displaying cursor if there has been no mouse movement
      base::TimeTicks now = tick_clock_->NowTicks();
      if ((now - last_mouse_movement_timestamp_) >
          base::TimeDelta::FromSeconds(MAX_IDLE_TIME_SECONDS)) {
        cursor_displayed_ = false;
        DVLOG(2) << "Turning off cursor display after idle time";
      }
    }
    if (!cursor_displayed_)
      return false;
  }

  const float x_scale =
      static_cast<float>(region_in_frame.width()) / view_size.width();
  const float y_scale =
      static_cast<float>(region_in_frame.height()) / view_size.height();

  gfx::NativeCursor cursor = GetLastKnownCursor();
  if (last_cursor_ != cursor || last_x_scale_ != x_scale ||
      last_y_scale_ != y_scale) {
    SkBitmap cursor_bitmap = GetLastKnownCursorImage(&last_cursor_hot_point_);
    const int scaled_width = cursor_bitmap.width() * x_scale;
    const int scaled_height = cursor_bitmap.height() * y_scale;
    if (scaled_width <= 0 || scaled_height <= 0) {
      DVLOG(2) << "scaled_width <= 0";
      Clear();
      return false;
    }
    scaled_cursor_bitmap_ = skia::ImageOperations::Resize(
        cursor_bitmap, skia::ImageOperations::RESIZE_BEST, scaled_width,
        scaled_height);

    last_cursor_ = cursor;
    last_x_scale_ = x_scale;
    last_y_scale_ = y_scale;
  }

  cursor_position.Offset(-last_cursor_hot_point_.x(),
                         -last_cursor_hot_point_.y());
  cursor_position_in_frame_ =
      gfx::Point(region_in_frame.x() + cursor_position.x() * x_scale,
                 region_in_frame.y() + cursor_position.y() * y_scale);
  return true;
}

// Helper function to composite a cursor bitmap on a YUV420 video frame.
void CursorRenderer::RenderOnVideoFrame(media::VideoFrame* target) const {
  if (scaled_cursor_bitmap_.isNull())
    return;

  DCHECK(target);

  gfx::Rect rect = gfx::IntersectRects(
      gfx::Rect(scaled_cursor_bitmap_.width(), scaled_cursor_bitmap_.height()) +
          gfx::Vector2d(cursor_position_in_frame_.x(),
                        cursor_position_in_frame_.y()),
      target->visible_rect());

  scaled_cursor_bitmap_.lockPixels();
  for (int y = rect.y(); y < rect.bottom(); ++y) {
    int cursor_y = y - cursor_position_in_frame_.y();
    uint8_t* yplane = target->data(media::VideoFrame::kYPlane) +
                      y * target->row_bytes(media::VideoFrame::kYPlane);
    uint8_t* uplane = target->data(media::VideoFrame::kUPlane) +
                      (y / 2) * target->row_bytes(media::VideoFrame::kUPlane);
    uint8_t* vplane = target->data(media::VideoFrame::kVPlane) +
                      (y / 2) * target->row_bytes(media::VideoFrame::kVPlane);
    for (int x = rect.x(); x < rect.right(); ++x) {
      int cursor_x = x - cursor_position_in_frame_.x();
      SkColor color = scaled_cursor_bitmap_.getColor(cursor_x, cursor_y);
      int alpha = SkColorGetA(color);
      int color_r = SkColorGetR(color);
      int color_g = SkColorGetG(color);
      int color_b = SkColorGetB(color);
      int color_y = clip_byte(
          ((color_r * 66 + color_g * 129 + color_b * 25 + 128) >> 8) + 16);
      yplane[x] = alpha_blend(alpha, color_y, yplane[x]);

      // Only sample U and V at even coordinates.
      if ((x % 2 == 0) && (y % 2 == 0)) {
        int color_u = clip_byte(
            ((color_r * -38 + color_g * -74 + color_b * 112 + 128) >> 8) + 128);
        int color_v = clip_byte(
            ((color_r * 112 + color_g * -94 + color_b * -18 + 128) >> 8) + 128);
        uplane[x / 2] = alpha_blend(alpha, color_u, uplane[x / 2]);
        vplane[x / 2] = alpha_blend(alpha, color_v, vplane[x / 2]);
      }
    }
  }
  scaled_cursor_bitmap_.unlockPixels();
}

void CursorRenderer::OnMouseMoved(const gfx::Point& location,
                                  base::TimeTicks timestamp) {
  if (!cursor_displayed_) {
    if (std::abs(location.x() - last_mouse_position_x_) > MIN_MOVEMENT_PIXELS ||
        std::abs(location.y() - last_mouse_position_y_) > MIN_MOVEMENT_PIXELS)
      cursor_displayed_ = true;
  }

  if (cursor_displayed_) {
    last_mouse_movement_timestamp_ = timestamp;
    last_mouse_position_x_ = location.x();
    last_mouse_position_y_ = location.y();
  }
}

void CursorRenderer::OnMouseClicked(const gfx::Point& location,
                                    base::TimeTicks timestamp) {
  cursor_displayed_ = true;
  last_mouse_movement_timestamp_ = timestamp;
  last_mouse_position_x_ = location.x();
  last_mouse_position_y_ = location.y();
}

}  // namespace content
