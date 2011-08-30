// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_DESKTOP_H_
#define AURA_DESKTOP_H_
#pragma once

#include "aura/root_window.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

namespace ui {
class Compositor;
}

namespace aura {

class DesktopHost;
class MouseEvent;

// Desktop is responsible for hosting a set of windows.
class Desktop {
 public:
  Desktop();
  ~Desktop();

  // Shows the desktop host.
  void Show();

  // Sets the size of the desktop.
  void SetSize(const gfx::Size& size);

  // Shows the desktop host and runs an event loop for it.
  void Run();

  // Draws the necessary set of windows.
  void Draw();

  // Handles a mouse event. Returns true if handled.
  bool OnMouseEvent(const MouseEvent& event);

  // Compositor we're drawing to.
  ui::Compositor* compositor() { return compositor_.get(); }

  Window* window() { return window_.get(); }

  static Desktop* GetInstance();

 private:
  scoped_refptr<ui::Compositor> compositor_;

  scoped_ptr<internal::RootWindow> window_;

  DesktopHost* host_;

  static Desktop* instance_;

  DISALLOW_COPY_AND_ASSIGN(Desktop);
};

}  // namespace aura

#endif  // AURA_DESKTOP_H_
