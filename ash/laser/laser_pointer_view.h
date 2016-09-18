// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LASER_LASER_POINTER_VIEW_H_
#define ASH_LASER_LASER_POINTER_VIEW_H_

#include <memory>

#include "ash/laser/laser_pointer_points.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace views {
class Widget;
}

namespace ash {

// LaserPointerView displays the palette tool laser pointer. It draws the laser,
// which consists of a point where the mouse cursor should be, as well as a
// trail of lines to help users track.
class LaserPointerView : public views::View {
 public:
  LaserPointerView(base::TimeDelta life_duration, aura::Window* root_window);
  ~LaserPointerView() override;

  void AddNewPoint(const gfx::Point& new_point);
  void Stop();
  aura::Window* GetRootWindow();

  // Reparents the widget if needed.
  void ReparentWidget(aura::Window* new_root_window);

 private:
  friend class LaserPointerControllerTestApi;

  // view::View:
  void OnPaint(gfx::Canvas* canvas) override;

  LaserPointerPoints laser_points_;
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerView);
};

}  // namespace ash

#endif  // ASH_LASER_LASER_POINTER_VIEW_H_
