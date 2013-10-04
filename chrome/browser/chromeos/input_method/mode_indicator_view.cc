// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/input_method/mode_indicator_view.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"

namespace chromeos {
namespace input_method {

ModeIndicatorView::ModeIndicatorView()
    : label_(NULL) {
  Init();
}

ModeIndicatorView::~ModeIndicatorView() {}

void ModeIndicatorView::SetLabelTextUtf8(const std::string& text_utf8) {
  DCHECK(label_);

  // TODO(komatsu): Call Layout() when we support resize the parent widget.
  label_->SetText(UTF8ToUTF16(text_utf8));
}


void ModeIndicatorView::Layout() {
  DCHECK(label_);

  // Set the label in the center of the contents bounds.
  const gfx::Rect cb = GetContentsBounds();
  const gfx::Size ps = label_->GetPreferredSize();
  label_->SetBounds(cb.x() + (cb.width() - ps.width()) / 2,
                    cb.y() + (cb.height() - ps.height()) / 2,
                    ps.width(),
                    ps.height());
}

gfx::Size ModeIndicatorView::GetPreferredSize() {
  DCHECK(label_);
  return label_->GetPreferredSize();
}

void ModeIndicatorView::Init() {
  views::BubbleBorder* border = new views::BubbleBorder(
      views::BubbleBorder::TOP_CENTER,
      views::BubbleBorder::NO_SHADOW,
      SK_ColorWHITE);
  set_border(border);
  set_background(new views::BubbleBackground(border));

  // "xxxx" reserves an enough size of boundary.
  // It would be nice to reserve in a different way.
  label_ = new views::Label(UTF8ToUTF16("xxxx"));
  label_->set_border(views::Border::CreateEmptyBorder(2, 2, 2, 2));
  label_->set_background(
      views::Background::CreateSolidBackground(
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_WindowBackground)));
  label_->SetBackgroundColor(label_->background()->get_color());

  // Pass the ownership.
  AddChildView(label_);
}

}  // namespace input_method
}  // namespace chromeos
