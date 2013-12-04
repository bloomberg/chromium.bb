// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_DELEGATE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_DELEGATE_VIEW_H_

#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/gfx/rect.h"
#include "ui/views/bubble/bubble_delegate.h"

namespace views {
class Label;
}  // namespace views

namespace chromeos {
namespace input_method {

class ModeIndicatorDelegateView : public views::BubbleDelegateView {
 public:
  ModeIndicatorDelegateView(const gfx::Rect& cursor_bounds,
                            const base::string16& label);
  virtual ~ModeIndicatorDelegateView();

  // Show the mode indicator then hide with fading animation.
  void ShowAndFadeOut();

  // views::BubbleDelegateView override:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 protected:
  // views::BubbleDelegateView override:
  virtual void Init() OVERRIDE;

 private:
  // Hide the window with fading animation.  This is called from
  // ShowAndFadeOut.
  void FadeOut();

  gfx::Rect cursor_bounds_;
  views::Label* label_view_;
  base::OneShotTimer<ModeIndicatorDelegateView> timer_;
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MODE_INDICATOR_DELEGATE_VIEW_H_
