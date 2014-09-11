// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_COMMON_ATHENA_FRAME_VIEW_H_
#define ATHENA_COMMON_ATHENA_FRAME_VIEW_H_

#include "ui/views/window/non_client_view.h"

namespace views {
class Widget;
}

namespace athena {

// A NonClientFrameView used for non activity window.
// TODO(oshima): Move this to athena/util and share the code.
class AthenaFrameView : public views::NonClientFrameView {
 public:
  // The frame class name.
  static const char kViewClassName[];

  explicit AthenaFrameView(views::Widget* frame);
  virtual ~AthenaFrameView();

  // views::NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE {}
  virtual void ResetWindowControls() OVERRIDE {}
  virtual void UpdateWindowIcon() OVERRIDE {}
  virtual void UpdateWindowTitle() OVERRIDE {}
  virtual void SizeConstraintsChanged() OVERRIDE {}

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void Layout() OVERRIDE {}

 private:
  gfx::Insets NonClientBorderInsets() const;

  virtual int NonClientTopBorderHeight() const;
  virtual int NonClientBorderThickness() const;

  // Not owned.
  views::Widget* frame_;

  DISALLOW_COPY_AND_ASSIGN(AthenaFrameView);
};

}  // namespace athena

#endif  // ATHENA_COMMON_ATHENA_FRAME_VIEW_H_
