// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_FRAME_H_
#define ASH_WM_WINDOW_FRAME_H_
#pragma once

#include "ash/ash_export.h"
#include "ui/aura/window.h"

namespace ash {

// Interface for clients implementing a window frame.  Implementors should
// add a pointer to this interface to each aura::Window, using the key above.
class ASH_EXPORT WindowFrame {
 public:
  virtual ~WindowFrame() {}

  // Called when the mouse enters or exits a top-level window.
  virtual void OnWindowHoverChanged(bool hovered) = 0;
};

// aura::Window property name for a pointer to the WindowFrame interface.
ASH_EXPORT extern const aura::WindowProperty<WindowFrame*>* const
    kWindowFrameKey;

}  // namespace ash

#endif  // ASH_WM_WINDOW_FRAME_H_
