// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_FIXED_SIZED_SCROLL_VIEW_H_
#define ASH_SYSTEM_TRAY_FIXED_SIZED_SCROLL_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

// A custom scroll-view that has a specified dimension.
class FixedSizedScrollView : public views::ScrollView {
 public:
  FixedSizedScrollView();
  virtual ~FixedSizedScrollView();

  void SetContentsView(views::View* view);

  // Change the fixed size of the view. Invalidates the layout (by calling
  // PreferredSizeChanged()).
  void SetFixedSize(const gfx::Size& size);

  void set_fixed_size(const gfx::Size& size) { fixed_size_ = size; }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;

 protected:
  // Overridden from views::View:
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  gfx::Size fixed_size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizedScrollView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_FIXED_SIZED_SCROLL_VIEW_H_
