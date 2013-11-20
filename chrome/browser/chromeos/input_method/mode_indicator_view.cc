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

  label_->SetText(UTF8ToUTF16(text_utf8));

  // If Layout is not called here, the size of the view is not updated.
  // TODO(komatsu): Investigate a proper way not to call Layout.
  Layout();
}

namespace {
const int kMinSize = 43;
}  // namespace

void ModeIndicatorView::Layout() {
  DCHECK(label_);

  // Resize label (simply use the preferred size).
  label_->SizeToPreferredSize();
  const gfx::Size label_size = label_->size();

  // Calculate the view's base size to fit to the label size or the
  // minimum size.
  gfx::Size base_size = label_size;
  base_size.SetToMax(gfx::Size(kMinSize, kMinSize));

  // Resize view considering the insets of the border.
  gfx::Size view_size = base_size;
  const gfx::Insets insets = border()->GetInsets();
  view_size.Enlarge(insets.width(), insets.height());
  SetSize(view_size);

  // Relocate label (center of the view considering the insets).
  const int x = insets.left() + (base_size.width()  - label_size.width())  / 2;
  const int y = insets.top()  + (base_size.height() - label_size.height()) / 2;
  label_->SetX(x);
  label_->SetY(y);
}

void ModeIndicatorView::Init() {
  // Initialize view.
  views::BubbleBorder* border = new views::BubbleBorder(
      views::BubbleBorder::TOP_CENTER,
      views::BubbleBorder::NO_SHADOW,
      SK_ColorWHITE);
  set_border(border);
  set_background(new views::BubbleBackground(border));

  // Initialize label.
  label_ = new views::Label;
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
