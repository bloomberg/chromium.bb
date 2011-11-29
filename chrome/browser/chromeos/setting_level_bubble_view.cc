// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/setting_level_bubble_view.h"

#include <string>

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/progress_bar.h"

using views::Background;
using views::View;
using views::Widget;

namespace {

// Bubble metrics.
const int kWidth = 350, kHeight = 90;
const int kPadding = 20;
const int kProgressBarWidth = 211;
const int kProgressBarHeight = 17;

}  // namespace

namespace chromeos {

SettingLevelBubbleView::SettingLevelBubbleView()
    : progress_bar_(new views::ProgressBar()),
      icon_(NULL) {
}

void SettingLevelBubbleView::Init(SkBitmap* icon, double level, bool enabled) {
  DCHECK(!icon_);
  DCHECK(icon);
  icon_ = icon;
  AddChildView(progress_bar_);
  progress_bar_->SetDisplayRange(0.0, 100.0);
  progress_bar_->EnableCanvasFlippingForRTLUI(true);
  EnableCanvasFlippingForRTLUI(true);
  SetLevel(level);
  SetEnabled(enabled);
}

void SettingLevelBubbleView::SetIcon(SkBitmap* icon) {
  DCHECK(icon);
  icon_ = icon;
  SchedulePaint();
}

void SettingLevelBubbleView::SetLevel(double level) {
  progress_bar_->SetValue(level);
}

void SettingLevelBubbleView::SetEnabled(bool enabled) {
  progress_bar_->SetEnabled(enabled);
}

void SettingLevelBubbleView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  canvas->DrawBitmapInt(*icon_, kPadding, (height() - icon_->height()) / 2);
}

void SettingLevelBubbleView::Layout() {
  progress_bar_->SetBounds(width() - kPadding - kProgressBarWidth,
                           (height() - kProgressBarHeight) / 2,
                           kProgressBarWidth, kProgressBarHeight);
}

gfx::Size SettingLevelBubbleView::GetPreferredSize() {
  return gfx::Size(kWidth, kHeight);
}

}  // namespace chromeos
