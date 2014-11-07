// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_ATHENA_FRAME_VIEW_H_
#define ATHENA_MAIN_ATHENA_FRAME_VIEW_H_

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
  ~AthenaFrameView() override;

  // views::NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const override;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  virtual int NonClientHitTest(const gfx::Point& point) override;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) override {}
  virtual void ResetWindowControls() override {}
  virtual void UpdateWindowIcon() override {}
  virtual void UpdateWindowTitle() override {}
  virtual void SizeConstraintsChanged() override {}

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() const override;
  virtual const char* GetClassName() const override;
  virtual void Layout() override {}

 private:
  gfx::Insets NonClientBorderInsets() const;

  virtual int NonClientTopBorderHeight() const;
  virtual int NonClientBorderThickness() const;

  // Not owned.
  views::Widget* frame_;

  DISALLOW_COPY_AND_ASSIGN(AthenaFrameView);
};

}  // namespace athena

#endif  // ATHENA_MAIN_ATHENA_FRAME_VIEW_H_
