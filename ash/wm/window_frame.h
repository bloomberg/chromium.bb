// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_FRAME_H_
#define ASH_WM_WINDOW_FRAME_H_
#pragma once

#include "ash/ash_export.h"

namespace aura_shell {

// aura::Window property name for a pointer to the WindowFrame interface.
ASH_EXPORT extern const char* const kWindowFrameKey;

// Interface for clients implementing a window frame.  Implementors should
// add a pointer to this interface to each aura::Window, using the key above.
class ASH_EXPORT WindowFrame {
 public:
  virtual ~WindowFrame() {}

  // Called when the mouse enters or exits a top-level window.
  virtual void OnWindowHoverChanged(bool hovered) = 0;
};

}  // namespace aura_shell

#endif  // ASH_WM_WINDOW_FRAME_H_
