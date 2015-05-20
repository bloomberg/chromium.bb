// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_HEADLESS_H_
#define COMPONENTS_VIEW_MANAGER_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_HEADLESS_H_

#include "base/macros.h"
#include "components/view_manager/native_viewport/platform_viewport.h"
#include "ui/gfx/geometry/rect.h"

namespace native_viewport {

class PlatformViewportHeadless : public PlatformViewport {
 public:
  ~PlatformViewportHeadless() override;

  static scoped_ptr<PlatformViewport> Create(Delegate* delegate);

 private:
  explicit PlatformViewportHeadless(Delegate* delegate);

  // Overridden from PlatformViewport:
  void Init(const gfx::Rect& bounds) override;
  void Show() override;
  void Hide() override;
  void Close() override;
  gfx::Size GetSize() override;
  void SetBounds(const gfx::Rect& bounds) override;

  Delegate* delegate_;
  mojo::ViewportMetricsPtr metrics_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportHeadless);
};

}  // namespace native_viewport

#endif  // COMPONENTS_VIEW_MANAGER_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_HEADLESS_H_
