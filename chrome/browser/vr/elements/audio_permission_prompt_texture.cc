// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/audio_permission_prompt_texture.h"

#include "base/i18n/case_conversion.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/render_text.h"

namespace vr {

namespace {

constexpr float kWidth = 0.63f;
constexpr float kHeight = 0.218f;
constexpr float kButtonHeight = 0.064f;
constexpr float kCornerRadius = 0.006f;
constexpr float kPadding = 0.028;
constexpr float kIconSize = 0.042;
constexpr float kFontSizePromptText = 0.028f;
constexpr float kTextTopMargin = 0.007f;
constexpr float kTextLeftMargin = 0.010f;
constexpr float kVerticalGap = 0.056f;
constexpr float kButtonsDistance = 0.014f;
constexpr float kFontSizePromptButtonText = 0.024f;
constexpr float kButtonRadius = 0.0035f;

constexpr float kButtonWidth = 0.162f;

}  // namespace

AudioPermissionPromptTexture::AudioPermissionPromptTexture() = default;

AudioPermissionPromptTexture::~AudioPermissionPromptTexture() = default;

void AudioPermissionPromptTexture::Draw(SkCanvas* sk_canvas,
                                        const gfx::Size& texture_size) {
  size_.set_width(texture_size.width());
  size_.set_height(texture_size.height());

  // background
  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  SkPaint back_paint;
  back_paint.setColor(background_color());
  sk_canvas->drawRoundRect(SkRect::MakeWH(size_.width(), size_.height()),
                           ToPixels(kCornerRadius), ToPixels(kCornerRadius),
                           back_paint);

  // Icon
  gfx::PointF icon_location(ToPixels(kPadding), ToPixels(kPadding));
  VectorIcon::DrawVectorIcon(canvas, vector_icons::kMicrophoneIcon,
                             ToPixels(kIconSize), icon_location, icon_color_);

  // Prompt description.
  auto text = l10n_util::GetStringUTF16(
      IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_DESCRIPTION);
  gfx::FontList fonts;
  GetFontList(ToPixels(kFontSizePromptText), text, &fonts);
  gfx::Rect prompt_text_size(size_.width(), 0);
  std::vector<std::unique_ptr<gfx::RenderText>> lines =
      PrepareDrawStringRect(text, fonts, foreground_color(), &prompt_text_size,
                            kTextAlignmentNone, kWrappingBehaviorWrap);
  canvas->Save();
  canvas->Translate(
      gfx::Vector2d(ToPixels(IsRTL() ? kPadding + kTextLeftMargin
                                     : kTextLeftMargin + kIconSize + kPadding),
                    ToPixels(kPadding + kTextTopMargin)));
  for (auto& render_text : lines)
    render_text->Draw(canvas);
  canvas->Restore();

  // Buttons
  SkPaint paint;
  gfx::Rect button_text_size(ToPixels(kButtonWidth), 0);
  float radius = ToPixels(kButtonRadius);
  GetFontList(ToPixels(kFontSizePromptButtonText), text, &fonts);

  // Secondary button area.
  text = base::i18n::ToUpper(l10n_util::GetStringUTF16(
      IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_ABORT_BUTTON));
  lines = PrepareDrawStringRect(
      text, fonts, secondary_button_colors_.foreground, &button_text_size,
      kTextAlignmentCenter, kWrappingBehaviorWrap);
  secondary_button_rect_.SetRect(
      ToPixels(kWidth - kPadding - kButtonsDistance - 2 * kButtonWidth),
      ToPixels(kPadding + kIconSize + kVerticalGap), ToPixels(kButtonWidth),
      ToPixels(kButtonHeight));
  paint.setColor(secondary_button_colors_.GetBackgroundColor(
      secondary_hovered_, secondary_pressed_));
  canvas->Save();
  canvas->Translate(
      gfx::Vector2d(secondary_button_rect_.x(), secondary_button_rect_.y()));
  sk_canvas->drawRoundRect(
      SkRect::MakeXYWH(0, 0, ToPixels(kButtonWidth), ToPixels(kButtonHeight)),
      radius, radius, paint);
  canvas->Translate(gfx::Vector2d(
      0, ToPixels(kButtonHeight) / 2 - button_text_size.height() / 2));
  for (auto& render_text : lines)
    render_text->Draw(canvas);
  canvas->Restore();

  // Primary button area.
  text = base::i18n::ToUpper(l10n_util::GetStringUTF16(
      IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_CONTINUE_BUTTON));
  button_text_size.set_size(gfx::Size(ToPixels(kButtonWidth), 0));
  lines = PrepareDrawStringRect(text, fonts, primary_button_colors_.foreground,
                                &button_text_size, kTextAlignmentCenter,
                                kWrappingBehaviorWrap);
  primary_button_rect_.SetRect(ToPixels(kWidth - kPadding - kButtonWidth),
                               ToPixels(kPadding + kIconSize + kVerticalGap),
                               ToPixels(kButtonWidth), ToPixels(kButtonHeight));
  paint.setColor(primary_button_colors_.GetBackgroundColor(primary_hovered_,
                                                           primary_pressed_));
  canvas->Save();
  canvas->Translate(
      gfx::Vector2d(primary_button_rect_.x(), primary_button_rect_.y()));
  sk_canvas->drawRoundRect(
      SkRect::MakeXYWH(0, 0, ToPixels(kButtonWidth), ToPixels(kButtonHeight)),
      radius, radius, paint);
  canvas->Translate(gfx::Vector2d(
      0, ToPixels(kButtonHeight) / 2 - button_text_size.height() / 2));
  for (auto& render_text : lines)
    render_text->Draw(canvas);
  canvas->Restore();
}

float AudioPermissionPromptTexture::ToPixels(float meters) const {
  return meters * size_.width() / kWidth;
}

gfx::PointF AudioPermissionPromptTexture::PercentToPixels(
    const gfx::PointF& percent) const {
  return gfx::PointF(percent.x() * size_.width(), percent.y() * size_.height());
}

bool AudioPermissionPromptTexture::HitsPrimaryButton(
    const gfx::PointF& position) const {
  return primary_button_rect_.Contains(PercentToPixels(position));
}

bool AudioPermissionPromptTexture::HitsSecondaryButton(
    const gfx::PointF& position) const {
  return secondary_button_rect_.Contains(PercentToPixels(position));
}

void AudioPermissionPromptTexture::SetPrimaryButtonHovered(bool hovered) {
  SetAndDirty(&primary_hovered_, hovered);
}

void AudioPermissionPromptTexture::SetPrimaryButtonPressed(bool pressed) {
  SetAndDirty(&primary_pressed_, pressed);
}

void AudioPermissionPromptTexture::SetSecondaryButtonHovered(bool hovered) {
  SetAndDirty(&secondary_hovered_, hovered);
}

void AudioPermissionPromptTexture::SetSecondaryButtonPressed(bool pressed) {
  SetAndDirty(&secondary_pressed_, pressed);
}

void AudioPermissionPromptTexture::SetPrimaryButtonColors(
    const ButtonColors& colors) {
  SetAndDirty(&primary_button_colors_, colors);
}

void AudioPermissionPromptTexture::SetSecondaryButtonColors(
    const ButtonColors& colors) {
  SetAndDirty(&secondary_button_colors_, colors);
}

void AudioPermissionPromptTexture::SetIconColor(SkColor color) {
  SetAndDirty(&icon_color_, color);
}

gfx::Size AudioPermissionPromptTexture::GetPreferredTextureSize(
    int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width * kHeight / kWidth);
}

gfx::SizeF AudioPermissionPromptTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr
