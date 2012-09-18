// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DRAW_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_ANDROID_DRAW_DELEGATE_IMPL_H_

#include "base/callback.h"
#include "content/public/browser/android/draw_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace content {

class DrawDelegateImpl : public DrawDelegate {
 public:
  static DrawDelegateImpl* GetInstance();
  DrawDelegateImpl();
  virtual ~DrawDelegateImpl();

  // DrawDelegate implementation.
  virtual void SetUpdateCallback(
      const SurfaceUpdatedCallback& callback) OVERRIDE;
  virtual void SetBounds(const gfx::Size& size) OVERRIDE;

  void OnSurfaceUpdated(uint64 texture, RenderWidgetHostView* view,
                        const SurfacePresentedCallback& present_callback);
  gfx::Size GetBounds() { return size_; }

 protected:
  SurfaceUpdatedCallback draw_callback_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(DrawDelegateImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DRAW_DELEGATE_IMPL_H_
