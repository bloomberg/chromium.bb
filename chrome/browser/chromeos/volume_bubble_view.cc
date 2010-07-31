// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/volume_bubble_view.h"

#include <string>

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "gfx/canvas.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/progress_bar.h"

using views::Background;
using views::View;
using views::Widget;

namespace {

// Volume bubble metrics.
const int kWidth = 300, kHeight = 75;
const int kMargin = 25;
const int kProgressBarHeight = 20;

}  // namespace

namespace chromeos {

VolumeBubbleView::VolumeBubbleView() : progress_bar_(NULL) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  volume_icon_ = rb.GetBitmapNamed(IDR_VOLUMEBUBBLE_ICON);
}

void VolumeBubbleView::Init(int volume_level_percent) {
  DCHECK(volume_level_percent >= 0 && volume_level_percent <= 100);
  progress_bar_ = new views::ProgressBar();
  AddChildView(progress_bar_);
  Update(volume_level_percent);
}

void VolumeBubbleView::Update(int volume_level_percent) {
  DCHECK(volume_level_percent >= 0 && volume_level_percent <= 100);
  progress_bar_->SetProgress(volume_level_percent);
}

void VolumeBubbleView::Paint(gfx::Canvas* canvas) {
  views::View::Paint(canvas);
  canvas->DrawBitmapInt(*volume_icon_,
                        volume_icon_->width(),
                        (height() - volume_icon_->height()) / 2);
}

void VolumeBubbleView::Layout() {
  progress_bar_->SetBounds(volume_icon_->width() + kMargin * 2,
                           (height() - kProgressBarHeight) / 2,
                           width() - volume_icon_->width() - kMargin * 3,
                           kProgressBarHeight);
}

gfx::Size VolumeBubbleView::GetPreferredSize() {
  return gfx::Size(kWidth, kHeight);
}

}  // namespace chromeos
