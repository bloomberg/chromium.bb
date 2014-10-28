// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_POSITION_CONTROLLER_H_
#define ASH_DISPLAY_SCREEN_POSITION_CONTROLLER_H_

#include "base/basictypes.h"
#include "ui/aura/client/screen_position_client.h"

namespace ash {

class ScreenPositionController : public aura::client::ScreenPositionClient {
 public:
  ScreenPositionController() {}
  ~ScreenPositionController() override {}

  // aura::client::ScreenPositionClient overrides:
  void ConvertPointToScreen(const aura::Window* window,
                            gfx::Point* point) override;
  void ConvertPointFromScreen(const aura::Window* window,
                              gfx::Point* point) override;
  void ConvertHostPointToScreen(aura::Window* window,
                                gfx::Point* point) override;
  void SetBounds(aura::Window* window,
                 const gfx::Rect& bounds,
                 const gfx::Display& display) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenPositionController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_POSITION_CONTROLLER_H_
