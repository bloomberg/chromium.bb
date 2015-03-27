// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PLATFORM_WINDOW_CAST_H_
#define CHROMECAST_PLATFORM_WINDOW_CAST_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window.h"

namespace chromecast {
namespace ozone {

class PlatformWindowCast : public ui::PlatformWindow {
 public:
  PlatformWindowCast(ui::PlatformWindowDelegate* delegate,
                     const gfx::Rect& bounds);
  ~PlatformWindowCast() override {}

  // PlatformWindow implementation:
  gfx::Rect GetBounds() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void Show() override {}
  void Hide() override {}
  void Close() override {}
  void SetCapture() override {}
  void ReleaseCapture() override {}
  void ToggleFullscreen() override {}
  void Maximize() override {}
  void Minimize() override {}
  void Restore() override {}
  void SetCursor(ui::PlatformCursor cursor) override {}
  void MoveCursorTo(const gfx::Point& location) override {}
  void ConfineCursorToBounds(const gfx::Rect& bounds) override {}

 private:
  ui::PlatformWindowDelegate* delegate_;
  gfx::Rect bounds_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(PlatformWindowCast);
};

}  // namespace ozone
}  // namespace chromecast

#endif  // CHROMECAST_PLATFORM_WINDOW_CAST_H_
