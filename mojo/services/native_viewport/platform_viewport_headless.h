// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "mojo/services/native_viewport/platform_viewport.h"
#include "ui/gfx/rect.h"

namespace mojo {

class PlatformViewportHeadless : public PlatformViewport {
 public:
  virtual ~PlatformViewportHeadless();

  static scoped_ptr<PlatformViewport> Create(Delegate* delegate);

 private:
  explicit PlatformViewportHeadless(Delegate* delegate);

  // Overridden from PlatformViewport:
  virtual void Init(const gfx::Rect& bounds) override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void Close() override;
  virtual gfx::Size GetSize() override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual void SetCapture() override;
  virtual void ReleaseCapture() override;

  Delegate* delegate_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportHeadless);
};

}  // namespace mojo
