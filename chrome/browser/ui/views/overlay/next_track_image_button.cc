// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/next_track_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

SkColor kNextTrackIconColor = SK_ColorWHITE;

}  // namespace

namespace views {

NextTrackImageButton::NextTrackImageButton(ButtonListener* listener)
    : ImageButton(listener) {
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);

  // Accessibility.
  SetFocusForPlatform();
  const base::string16 next_track_button_label(l10n_util::GetStringUTF16(
      IDS_PICTURE_IN_PICTURE_NEXT_TRACK_CONTROL_ACCESSIBLE_TEXT));
  SetAccessibleName(next_track_button_label);
  SetTooltipText(next_track_button_label);
  SetInstallFocusRingOnFocus(true);
}

NextTrackImageButton::~NextTrackImageButton() = default;

gfx::Size NextTrackImageButton::GetLastVisibleSize() const {
  return size().IsEmpty() ? last_visible_size_ : size();
}

void NextTrackImageButton::OnBoundsChanged(const gfx::Rect&) {
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(vector_icons::kMediaNextTrackIcon,
                                 GetLastVisibleSize().width() / 2,
                                 kNextTrackIconColor));
}

void NextTrackImageButton::ToggleVisibility(bool is_visible) {
  if (is_visible && !size().IsEmpty()) {
    last_visible_size_ = size();
  }

  SetVisible(is_visible);
  SetEnabled(is_visible);
  SetSize(is_visible ? GetLastVisibleSize() : gfx::Size());
}

}  // namespace views
