// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OVERLAY_OVERLAY_WINDOW_H_
#define CHROME_BROWSER_UI_OVERLAY_OVERLAY_WINDOW_H_

#include "base/memory/ptr_util.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}

namespace ui {
class Layer;
}

// This window will always float above other windows. The intention is to show
// content perpetually while the user is still interacting with the other
// browser windows.
class OverlayWindow {
 public:
  OverlayWindow() = default;
  virtual ~OverlayWindow() = default;

  // Returns a created OverlayWindow. This is defined in the platform-specific
  // implementation for the class.
  static std::unique_ptr<OverlayWindow> Create();

  virtual void Init() = 0;
  virtual bool IsActive() const = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Close() = 0;
  virtual void Activate() = 0;
  virtual bool IsVisible() = 0;
  virtual bool IsAlwaysOnTop() const = 0;
  virtual ui::Layer* GetLayer() = 0;
  virtual gfx::NativeWindow GetNativeWindow() const = 0;
  // Retrieves the window's current bounds, including its window.
  virtual gfx::Rect GetBounds() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayWindow);
};

#endif  // CHROME_BROWSER_UI_OVERLAY_OVERLAY_WINDOW_H_
