// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/status_area_button.h"

#include "content/common/notification_service.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "views/border.h"
#include "views/view.h"

namespace chromeos {

// Colors for different text styles.
static const SkColor kWhitePlainTextColor = 0x99ffffff;
static const SkColor kWhiteHaloedTextColor = 0xb3ffffff;
static const SkColor kWhiteHaloedHaloColor = 0xb3000000;
static const SkColor kGrayEmbossedTextColor = 0xff4c4c4c;
static const SkColor kGrayEmbossedShadowColor = 0x80ffffff;

// Status area font is bigger.
const int kFontSizeDelta = 1;

////////////////////////////////////////////////////////////////////////////////
// StatusAreaButton

StatusAreaButton::StatusAreaButton(StatusAreaHost* host,
                                   views::ViewMenuDelegate* menu_delegate)
    : MenuButton(NULL, std::wstring(), menu_delegate, false),
      use_menu_button_paint_(false),
      active_(true),
      host_(host) {
  set_border(NULL);
  set_use_menu_button_paint(true);
  gfx::Font font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
  font = font.DeriveFont(3, gfx::Font::BOLD);
  SetFont(font);
  SetShowMultipleIconStates(false);
  set_alignment(TextButton::ALIGN_CENTER);
  set_border(NULL);

  // Use an offset that is top aligned with toolbar.
  set_menu_offset(0, 4);

  UpdateTextStyle();
}

void StatusAreaButton::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  if (state() == BS_PUSHED) {
    // Apply 10% white when pushed down.
    canvas->FillRectInt(SkColorSetARGB(0x19, 0xFF, 0xFF, 0xFF),
        0, 0, width(), height());
  }

  if (use_menu_button_paint_) {
    views::MenuButton::PaintButton(canvas, mode);
  } else {
    canvas->DrawBitmapInt(icon(), horizontal_padding(), 0);
    OnPaintFocusBorder(canvas);
  }
}

void StatusAreaButton::SetText(const std::wstring& text) {
  // TextButtons normally remember the max text size, so the button's preferred
  // size will always be as large as the largest text ever put in it.
  // We clear that max text size, so we can adjust the size to fit the text.
  // The order is important.  ClearMaxTextSize sets the size to that of the
  // current text, so it must be called after SetText.
  views::MenuButton::SetText(text);
  ClearMaxTextSize();
  PreferredSizeChanged();
}

bool StatusAreaButton::Activate() {
  if (active_) {
    return views::MenuButton::Activate();
  } else {
    return true;
  }
}

gfx::Size StatusAreaButton::GetPreferredSize() {
  gfx::Insets insets = views::MenuButton::GetInsets();
  gfx::Size prefsize(icon_width() + insets.width(),
                     icon_height() + insets.height());

  // Adjusts size when use menu button paint.
  if (use_menu_button_paint_) {
    gfx::Size menu_button_size = views::MenuButton::GetPreferredSize();
    prefsize.SetSize(
      std::max(prefsize.width(), menu_button_size.width()),
      std::max(prefsize.height(), menu_button_size.height())
    );

    // Shift 1-pixel down for odd number of pixels in vertical space.
    if ((prefsize.height() - menu_button_size.height()) % 2) {
      insets_.Set(insets.top() + 1, insets.left(),
          insets.bottom(), insets.right());
    }
  }

  // Add padding.
  prefsize.Enlarge(2 * horizontal_padding(), 0);

  return prefsize;
}

gfx::Insets StatusAreaButton::GetInsets() const {
  return insets_;
}

void StatusAreaButton::OnThemeChanged() {
  UpdateTextStyle();
}

void StatusAreaButton::UpdateTextStyle() {
  ClearEmbellishing();
  switch (host_->GetTextStyle()) {
    case StatusAreaHost::kWhitePlain:
      SetEnabledColor(kWhitePlainTextColor);
      break;
    case StatusAreaHost::kWhiteHaloed:
      SetEnabledColor(kWhiteHaloedTextColor);
      SetTextHaloColor(kWhiteHaloedHaloColor);
      break;
    case StatusAreaHost::kGrayEmbossed:
      SetEnabledColor(kGrayEmbossedTextColor);
      SetTextShadowColors(kGrayEmbossedShadowColor, kGrayEmbossedShadowColor);
      SetTextShadowOffset(0, 1);
      break;
  }
}

}  // namespace chromeos
