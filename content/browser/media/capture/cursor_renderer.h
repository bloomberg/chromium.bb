// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "content/common/content_export.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// Setting to control cursor display based on either mouse movement or always
// forced to be enabled.
enum CursorDisplaySetting {
  kCursorAlwaysEnabled,
  kCursorEnabledOnMouseMovement
};

// CursorRenderer is an abstract base class that handles all the
// non-platform-specific common cursor rendering functionality.
//
// In order to track the cursor, the platform-specific implementation
// will listen to mouse events.
class CONTENT_EXPORT CursorRenderer {
 public:
  static std::unique_ptr<CursorRenderer> Create(gfx::NativeView view);

  CursorRenderer(gfx::NativeView captured_view,
                 CursorDisplaySetting cursor_display);

  virtual ~CursorRenderer();

  // Clears the cursor state being tracked. Called when there is a need to
  // reset the state.
  void Clear();

  // Takes a snapshot of the current cursor state and determines whether
  // the cursor will be rendered, which cursor image to render, and at what
  // location within |region_in_frame| to render it. Returns true if cursor
  // needs to be rendered.
  bool SnapshotCursorState(const gfx::Rect& region_in_frame);

  // Renders cursor on the |target| video frame.
  void RenderOnVideoFrame(media::VideoFrame* target) const;

  // Returns a weak pointer.
  base::WeakPtr<CursorRenderer> GetWeakPtr();

 protected:
  enum {
    // Minium movement before cursor is rendered on frame.
    MIN_MOVEMENT_PIXELS = 15,
    // Maximum idle time allowed before we stop rendering the cursor on frame.
    MAX_IDLE_TIME_SECONDS = 2
  };

  // Returns true if the captured view is a part of an active application
  // window.
  virtual bool IsCapturedViewActive() = 0;

  // Returns the size of the captured view (view coordinates).
  virtual gfx::Size GetCapturedViewSize() = 0;

  // Returns the cursor's position within the captured view (view coordinates).
  virtual gfx::Point GetCursorPositionInView() = 0;

  // Returns the last-known mouse cursor.
  virtual gfx::NativeCursor GetLastKnownCursor() = 0;

  // Returns the image of the last-known mouse cursor and its hotspot.
  virtual SkBitmap GetLastKnownCursorImage(gfx::Point* hot_point) = 0;

  // Called by subclasses to report mouse events within the captured view.
  void OnMouseMoved(const gfx::Point& location, base::TimeTicks timestamp);
  void OnMouseClicked(const gfx::Point& location, base::TimeTicks timestamp);

 private:
  friend class CursorRendererAuraTest;
  friend class CursorRendererMacTest;

  const gfx::NativeView captured_view_;

  // Snapshot of cursor, source size, scale, position, and cursor bitmap;
  // as of the last call to SnapshotCursorState.
  float last_x_scale_;
  float last_y_scale_;
  gfx::NativeCursor last_cursor_;
  gfx::Point cursor_position_in_frame_;
  gfx::Point last_cursor_hot_point_;
  SkBitmap scaled_cursor_bitmap_;

  // Updated in mouse event listener and used to make a decision on
  // when the cursor is rendered.
  base::TimeTicks last_mouse_movement_timestamp_;
  float last_mouse_position_x_;
  float last_mouse_position_y_;
  bool cursor_displayed_;

  // Controls whether cursor is displayed based on active mouse movement.
  CursorDisplaySetting cursor_display_setting_;

  // Allows tests to replace the clock.
  base::DefaultTickClock default_tick_clock_;
  base::TickClock* tick_clock_;

  base::WeakPtrFactory<CursorRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CursorRenderer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_CURSOR_RENDERER_H_
