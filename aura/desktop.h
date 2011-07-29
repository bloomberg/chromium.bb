// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_DESKTOP_H_
#define AURA_DESKTOP_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

namespace ui {
class Compositor;
class Window;
}

namespace aura {

// Desktop is responsible for hosting a set of windows.
class Desktop {
 public:
  Desktop(gfx::AcceleratedWidget widget, const gfx::Size& size);
  ~Desktop();

  // Draws the necessary set of windows.
  void Draw();

  // Compositor we're drawing to.
  ui::Compositor* compositor() { return compositor_.get(); }

 private:
  friend class Window;

  typedef std::vector<Window*> Windows;

  // Methods invoked by Window.
  // TODO: move these into an interface that Window uses to talk to Desktop.
  void AddWindow(Window* window);
  void RemoveWindow(Window* window);

  scoped_refptr<ui::Compositor> compositor_;

  // The windows. Topmost window is last.
  std::vector<Window*> windows_;

  DISALLOW_COPY_AND_ASSIGN(Desktop);
};

}  // namespace aura

#endif  // AURA_DESKTOP_H_
