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
  virtual ~ScreenPositionController() {}

  // aura::client::ScreenPositionClient overrides:
  virtual void ConvertPointToScreen(const aura::Window* window,
                                    gfx::Point* point) OVERRIDE;
  virtual void ConvertPointFromScreen(const aura::Window* window,
                                      gfx::Point* point) OVERRIDE;
  virtual void ConvertHostPointToScreen(aura::Window* window,
                                        gfx::Point* point) OVERRIDE;
  virtual void SetBounds(aura::Window* window,
                         const gfx::Rect& bounds,
                         const gfx::Display& display) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenPositionController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_POSITION_CONTROLLER_H_
