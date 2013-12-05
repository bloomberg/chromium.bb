// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mode_indicator_delegate_view.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace chromeos {
namespace input_method {

namespace {
// Minimum size of inner contents in pixel.
// 43 is the designed size including the default margin (6 * 2).
const int kMinSize = 31;

// After this duration in msec, the mode inicator will be fading out.
const int kShowingDuration = 500;
}  // namespace


ModeIndicatorDelegateView::ModeIndicatorDelegateView(
    const gfx::Rect& cursor_bounds,
    const base::string16& label)
  : cursor_bounds_(cursor_bounds),
    label_view_(new views::Label(label)) {
  set_use_focusless(true);
  set_accept_events(false);
  set_parent_window(
      ash::Shell::GetContainer(
          ash::wm::GetActiveWindow()->GetRootWindow(),
          ash::internal::kShellWindowId_InputMethodContainer));
  set_shadow(views::BubbleBorder::NO_SHADOW);
  set_arrow(views::BubbleBorder::TOP_CENTER);
}

ModeIndicatorDelegateView::~ModeIndicatorDelegateView() {}

void ModeIndicatorDelegateView::FadeOut() {
  StartFade(false);
}

void ModeIndicatorDelegateView::ShowAndFadeOut() {
  views::corewm::SetWindowVisibilityAnimationTransition(
      GetWidget()->GetNativeView(),
      views::corewm::ANIMATE_HIDE);
  GetWidget()->Show();
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kShowingDuration),
               this,
               &ModeIndicatorDelegateView::FadeOut);
}

gfx::Size ModeIndicatorDelegateView::GetPreferredSize() {
  gfx::Size size = label_view_->GetPreferredSize();
  size.SetToMax(gfx::Size(kMinSize, kMinSize));
  return size;
}

void ModeIndicatorDelegateView::Init() {
  SetLayoutManager(new views::FillLayout());
  AddChildView(label_view_);

  SetAnchorRect(cursor_bounds_);
}

}  // namespace input_method
}  // namespace chromeos
