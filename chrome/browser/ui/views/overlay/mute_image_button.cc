// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/mute_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

const int kMuteImageSize = 22;

constexpr SkColor kMuteIconColor = SK_ColorWHITE;

}  // namespace

namespace views {

MuteImageButton::MuteImageButton(ButtonListener* listener)
    : ImageButton(listener),
      mute_image_(
          gfx::CreateVectorIcon(kTabAudioIcon, kMuteImageSize, kMuteIconColor)),
      unmute_image_(gfx::CreateVectorIcon(kTabAudioMutingIcon,
                                          kMuteImageSize,
                                          kMuteIconColor)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  // Accessibility.
  SetFocusForPlatform();
  const base::string16 mute_button_label(l10n_util::GetStringUTF16(
      IDS_PICTURE_IN_PICTURE_MUTE_CONTROL_ACCESSIBLE_TEXT));
  SetAccessibleName(mute_button_label);
  SetInstallFocusRingOnFocus(true);
}

void MuteImageButton::SetMutedState(
    OverlayWindowViews::MutedState muted_state) {
  muted_state_ = muted_state;
  switch (muted_state_) {
    case OverlayWindowViews::kMuted:
      SetImage(views::Button::STATE_NORMAL, unmute_image_);
      SetTooltipText(l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_UNMUTE_CONTROL_TEXT));
      break;
    case OverlayWindowViews::kUnmuted:
      SetImage(views::Button::STATE_NORMAL, mute_image_);
      SetTooltipText(
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_MUTE_CONTROL_TEXT));
      break;
    case OverlayWindowViews::kNoAudio:
      ToggleVisibility(false);
  }
  SchedulePaint();
}

gfx::Size MuteImageButton::GetLastVisibleSize() const {
  return size().IsEmpty() ? last_visible_size_ : size();
}

void MuteImageButton::OnBoundsChanged(const gfx::Rect&) {
  if (!size().IsEmpty())
    last_visible_size_ = size();
}

void MuteImageButton::ToggleVisibility(bool is_visible) {
  if (muted_state_ == OverlayWindowViews::kNoAudio && is_visible)
    return;

  layer()->SetVisible(is_visible);
  SetEnabled(is_visible);
  SetSize(is_visible ? GetLastVisibleSize() : gfx::Size());
}

}  // namespace views
