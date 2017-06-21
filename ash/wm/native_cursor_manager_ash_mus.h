// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_NATIVE_CURSOR_MANAGER_ASH_MUS_H_
#define ASH_WM_NATIVE_CURSOR_MANAGER_ASH_MUS_H_

#include "ash/wm/native_cursor_manager_ash.h"

namespace ui {
class CursorDataFactoryOzone;
class ImageCursors;
}  // namespace ui

namespace ash {

// An NativeCursorManagerAsh which is used in Mushrome mode.
//
// NativeCursorManagerAshClassic implicitly communicates with ozone via
// ImageCursors and the window tree host, but in mushrome, we want to
// communicate with the window server purely through the mus interfaces.
//
// This doesn't mean that we can just cut out all ozone communication; we'll
// have to do what the mash does, which is install just the cursor factory part
// of ozone.
class ASH_EXPORT NativeCursorManagerAshMus : public NativeCursorManagerAsh {
 public:
  NativeCursorManagerAshMus();
  ~NativeCursorManagerAshMus() override;

 private:
  // Overridden from NativeCursorManagerAsh:
  void SetNativeCursorEnabled(bool enabled) override;
  float GetScale() const override;
  display::Display::Rotation GetRotation() const override;

  // Overridden from ::wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetCursor(gfx::NativeCursor cursor,
                 ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetVisibility(bool visible,
                     ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetCursorSet(ui::CursorSetType cursor_set,
                    ::wm::NativeCursorManagerDelegate* delegate) override;
  void SetMouseEventsEnabled(
      bool enabled,
      ::wm::NativeCursorManagerDelegate* delegate) override;

  // The cursor location where the cursor was disabled.
  gfx::Point disabled_cursor_location_;

  bool native_cursor_enabled_ = true;

#if defined(USE_OZONE)
  std::unique_ptr<ui::CursorDataFactoryOzone> cursor_factory_ozone_;
#endif

  // Always nullptr when USE_OZONE is false.
  std::unique_ptr<::ui::ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(NativeCursorManagerAshMus);
};

}  // namespace ash

#endif  // ASH_WM_NATIVE_CURSOR_MANAGER_ASH_MUS_H_
