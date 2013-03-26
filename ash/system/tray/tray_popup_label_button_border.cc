// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_popup_label_button_border.h"

#include "base/i18n/rtl.h"
#include "grit/ash_resources.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/vector2d.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/native_theme_delegate.h"

namespace ash {
namespace internal {

namespace {

const int kTrayPopupLabelButtonPaddingHorizontal = 16;
const int kTrayPopupLabelButtonPaddingVertical = 8;

const int kTrayPopupLabelButtonBorderImagesNormal[] = {
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
};

const int kTrayPopupLabelButtonBorderImagesHovered[] = {
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_HOVER_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
};

}  // namespace

TrayPopupLabelButtonBorder::TrayPopupLabelButtonBorder()
    : LabelButtonBorder(views::Button::STYLE_TEXTBUTTON) {
  SetPainter(false, views::Button::STATE_NORMAL,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesNormal));
  SetPainter(false, views::Button::STATE_DISABLED,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesNormal));
  SetPainter(false, views::Button::STATE_HOVERED,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesHovered));
  SetPainter(false, views::Button::STATE_PRESSED,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesHovered));
}

TrayPopupLabelButtonBorder::~TrayPopupLabelButtonBorder() {}

void TrayPopupLabelButtonBorder::Paint(const views::View& view,
                                       gfx::Canvas* canvas) {
  const views::NativeThemeDelegate* native_theme_delegate =
      static_cast<const views::LabelButton*>(&view);
  ui::NativeTheme::ExtraParams extra;
  const ui::NativeTheme::State state =
      native_theme_delegate->GetThemeState(&extra);
  if (state == ui::NativeTheme::kNormal ||
      state == ui::NativeTheme::kDisabled) {
    // In normal and disabled state, the border is a vertical bar separating the
    // button from the preceding sibling. If this button is its parent's first
    // visible child, the separator bar should be omitted.
    const views::View* first_visible_child = NULL;
    for (int i = 0; i < view.parent()->child_count(); ++i) {
      const views::View* child = view.parent()->child_at(i);
      if (child->visible()) {
        first_visible_child = child;
        break;
      }
    }
    if (first_visible_child == &view)
      return;
  }
  if (base::i18n::IsRTL()) {
    canvas->Save();
    canvas->Translate(gfx::Vector2d(view.width(), 0));
    canvas->Scale(-1, 1);
    LabelButtonBorder::Paint(view, canvas);
    canvas->Restore();
  } else {
    LabelButtonBorder::Paint(view, canvas);
  }
}

gfx::Insets TrayPopupLabelButtonBorder::GetInsets() const {
  return gfx::Insets(kTrayPopupLabelButtonPaddingVertical,
                     kTrayPopupLabelButtonPaddingHorizontal,
                     kTrayPopupLabelButtonPaddingVertical,
                     kTrayPopupLabelButtonPaddingHorizontal);
}

}  // namespace internal
}  // namespace ash
