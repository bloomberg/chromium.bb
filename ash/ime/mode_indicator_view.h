// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_MODE_INDICATOR_VIEW_H_
#define ASH_IME_MODE_INDICATOR_VIEW_H_

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/bubble/bubble_delegate.h"

namespace views {
class Label;
}  // namespace views

namespace ash {
namespace ime {

class ASH_EXPORT ModeIndicatorView : public views::BubbleDelegateView {
 public:
  ModeIndicatorView(gfx::NativeView parent,
                    const gfx::Rect& cursor_bounds,
                    const base::string16& label);
  virtual ~ModeIndicatorView();

  // Show the mode indicator then hide with fading animation.
  void ShowAndFadeOut();

  // views::BubbleDelegateView override:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 protected:
  // views::BubbleDelegateView override:
  virtual void Init() OVERRIDE;

  // views::WidgetDelegateView overrides:
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;

 private:
  // Hide the window with fading animation.  This is called from
  // ShowAndFadeOut.
  void FadeOut();

  gfx::Rect cursor_bounds_;
  views::Label* label_view_;
  base::OneShotTimer<ModeIndicatorView> timer_;
};

}  // namespace ime
}  // namespace ash

#endif  // ASH_IME_MODE_INDICATOR_VIEW_H_
