// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oobe_progress_bar.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"

namespace chromeos {

// static
SkBitmap* OobeProgressBar::dot_current_ = NULL;
SkBitmap* OobeProgressBar::dot_empty_ = NULL;
SkBitmap* OobeProgressBar::dot_filled_ = NULL;
SkBitmap* OobeProgressBar::line_ = NULL;
SkBitmap* OobeProgressBar::line_left_ = NULL;
SkBitmap* OobeProgressBar::line_right_ = NULL;

static const SkColor kCurrentTextColor = SkColorSetRGB(255, 255, 255);
static const SkColor kEmptyTextColor = SkColorSetRGB(112, 115, 118);
static const SkColor kFilledTextColor = SkColorSetRGB(112, 115, 118);
static const int kTextPadding = 5;

OobeProgressBar::OobeProgressBar(const std::vector<int>& steps)
    : steps_(steps), progress_(0) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  font_ = rb.GetFont(ResourceBundle::BaseFont);
  InitClass();
}

// static
void OobeProgressBar::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    // Load images.
    dot_current_ = rb.GetBitmapNamed(IDR_OOBE_PROGRESS_DOT_CURRENT);
    dot_empty_ = rb.GetBitmapNamed(IDR_OOBE_PROGRESS_DOT_EMPTY);
    dot_filled_ = rb.GetBitmapNamed(IDR_OOBE_PROGRESS_DOT_FILLED);
    line_ = rb.GetBitmapNamed(IDR_OOBE_PROGRESS_LINE);
    line_left_ = rb.GetBitmapNamed(IDR_OOBE_PROGRESS_LINE_LEFT);
    line_right_ = rb.GetBitmapNamed(IDR_OOBE_PROGRESS_LINE_RIGHT);

    initialized = true;
  }
}

void OobeProgressBar::Paint(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetLocalBounds();

  int x = bounds.x();
  int y = bounds.y();

  double step_width = static_cast<double>(bounds.width()) / steps_.size();

  for (size_t i = 0; i < steps_.size(); ++i) {
    SkBitmap* dot;
    SkColor color;
    SkBitmap* line_before = line_;
    SkBitmap* line_after = line_;
    if (i < progress_) {
      dot = dot_filled_;
      color = kFilledTextColor;
    } else if (i == progress_) {
      dot = dot_current_;
      color = kCurrentTextColor;
      line_before = line_left_;
      line_after = line_right_;
    } else {
      dot = dot_empty_;
      color = kEmptyTextColor;
    }

    // x coordinate for next step.
    int next_x = static_cast<int>((i + 1) * step_width);

    // Offset of line origin from dot origin.
    int line_offset_y = (dot->height() - line_->height()) / 2;

    // Current x for painting.
    int ix = x;

    int line_width = ((next_x - x) -
        (line_before->width() + dot->width() + line_after->width())) / 2;
    if (i > 0) {
      canvas->TileImageInt(*line_, ix, y + line_offset_y,
                           line_width, line_->height());
    }
    ix += line_width;
    if (i > 0) {
      canvas->DrawBitmapInt(*line_before, ix, y + line_offset_y);
    }
    ix += line_before->width();

    canvas->DrawBitmapInt(*dot, ix, y);
    ix += dot->width();

    if (i != steps_.size() - 1)
      canvas->DrawBitmapInt(*line_after, ix, y + line_offset_y);
    ix += line_after->width();

    if (i != steps_.size() - 1) {
      canvas->TileImageInt(*line_, ix, y + line_offset_y,
                           next_x - ix, line_->height());
    }

    string16 str = l10n_util::GetStringUTF16(steps_[i]);
    canvas->DrawStringInt(str, font_, color,
        x + kTextPadding, y + dot->height() + kTextPadding,
        (next_x - x - 2 * kTextPadding),
        (bounds.height() - dot->height() - 2 * kTextPadding),
        gfx::Canvas::MULTI_LINE | gfx::Canvas::TEXT_ALIGN_CENTER |
            gfx::Canvas::TEXT_VALIGN_TOP);

    x = next_x;
  }
}

void OobeProgressBar::OnLocaleChanged() {
  SchedulePaint();
}

void OobeProgressBar::SetStep(int step) {
  for (size_t i = 0; i < steps_.size(); ++i) {
    if (steps_[i] == step) {
      progress_ = i;
      SchedulePaint();
      return;
    }
  }
  NOTREACHED();
}

}  // namespace chromeos
