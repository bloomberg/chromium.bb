// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "mojo/services/native_viewport/platform_viewport.h"
#include "ui/gfx/rect.h"

namespace mojo {

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
  void SetCapture() override;
  void ReleaseCapture() override;

  Delegate* delegate_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportHeadless);
};

}  // namespace mojo
