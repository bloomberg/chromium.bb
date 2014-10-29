// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_AURA_IMAGE_WINDOW_DELEGATE_H_
#define CONTENT_BROWSER_WEB_CONTENTS_AURA_IMAGE_WINDOW_DELEGATE_H_

#include "content/common/content_export.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"

namespace content {

// An ImageWindowDelegate paints an image for a Window. The delegate destroys
// itself when the Window is destroyed. The delegate does not consume any event.
class CONTENT_EXPORT ImageWindowDelegate : public aura::WindowDelegate {
 public:
  ImageWindowDelegate();

  void SetImage(const gfx::Image& image);
  bool has_image() const { return !image_.IsEmpty(); }

 protected:
  ~ImageWindowDelegate() override;

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(gfx::Path* mask) const override;

 protected:
  gfx::Image image_;
  gfx::Size window_size_;

  // Keeps track of whether the window size matches the image size or not. If
  // the image size is smaller than the window size, then the delegate paints a
  // white background for the missing regions.
  bool size_mismatch_;

  DISALLOW_COPY_AND_ASSIGN(ImageWindowDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_AURA_IMAGE_WINDOW_DELEGATE_H_
