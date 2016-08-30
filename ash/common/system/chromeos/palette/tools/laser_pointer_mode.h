// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_TOOLS_LASER_POINTER_MODE_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_TOOLS_LASER_POINTER_MODE_H_

#include "ash/ash_export.h"
#include "ash/common/system/chromeos/palette/common_palette_tool.h"
#include "ui/views/pointer_watcher.h"

namespace ash {

class LaserPointerView;

// Controller for the laser pointer functionality. Enables/disables laser
// pointer as well as receives points and passes them off to be rendered.
class ASH_EXPORT LaserPointerMode : public CommonPaletteTool,
                                    public views::PointerWatcher {
 public:
  explicit LaserPointerMode(Delegate* delegate);
  ~LaserPointerMode() override;

 private:
  friend class LaserPointerModeTestApi;

  // PaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  void OnDisable() override;
  gfx::VectorIconId GetActiveTrayIcon() override;
  views::View* CreateView() override;

  // CommonPaletteTool:
  gfx::VectorIconId GetPaletteIconId() override;

  // views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              views::Widget* target) override;

  void StopTimer();

  // Timer callback which adds a point where the mouse was last seen. This
  // allows the trail to fade away when the mouse is stationary.
  void AddStationaryPoint();

  // Timer which will add a new stationary point when the mouse stops moving.
  // This will remove points that are too old.
  std::unique_ptr<base::Timer> timer_;
  int timer_repeat_count_ = 0;

  gfx::Point current_mouse_location_;
  std::unique_ptr<LaserPointerView> laser_pointer_view_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerMode);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_TOOLS_LASER_POINTER_MODE_H_
