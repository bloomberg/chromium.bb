// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  virtual void Init(const gfx::Rect& bounds) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;

  Delegate* delegate_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportHeadless);
};

}  // namespace mojo
