// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/setting_level_bubble_view.h"

#include <string>

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/canvas.h"
#include "views/controls/progress_bar.h"

using views::Background;
using views::View;
using views::Widget;

namespace {

// Bubble metrics.
const int kWidth = 350, kHeight = 100;
const int kPadding = 20;
const int kProgressBarWidth = 211;
const int kProgressBarHeight = 17;

}  // namespace

namespace chromeos {

SettingLevelBubbleView::SettingLevelBubbleView()
    : progress_bar_(NULL),
      icon_(NULL) {
}

void SettingLevelBubbleView::Init(SkBitmap* icon, int level_percent) {
  DCHECK(icon);
  DCHECK(level_percent >= 0 && level_percent <= 100);
  icon_ = icon;
  progress_bar_ = new views::ProgressBar();
  AddChildView(progress_bar_);
  Update(level_percent);
}

void SettingLevelBubbleView::SetIcon(SkBitmap* icon) {
  DCHECK(icon);
  icon_ = icon;
  SchedulePaint();
}

void SettingLevelBubbleView::Update(int level_percent) {
  DCHECK(level_percent >= 0 && level_percent <= 100);
  progress_bar_->SetProgress(level_percent);
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
