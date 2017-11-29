// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/exit_prompt_texture.h"

#include "base/i18n/case_conversion.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/render_text.h"

namespace vr {

namespace {

constexpr float kWidth = 0.672f;
constexpr float kHeight = 0.2f;
constexpr float kButtonWidth = 0.162f;
constexpr float kButtonHeight = 0.066f;
constexpr float kPromptTextButtonSeperatorHeight = 0.04f;
constexpr float kButtonsSeperatorWidth = 0.01f;
constexpr float kButtonRadiusFactor = 0.006f;
constexpr float kFontSizePromptText = 0.027f;
constexpr float kFontSizePromptButtonText = 0.024f;

}  // namespace

ExitPromptTexture::ExitPromptTexture() = default;

ExitPromptTexture::~ExitPromptTexture() = default;

void ExitPromptTexture::Draw(SkCanvas* sk_canvas,
                             const gfx::Size& texture_size) {
  DCHECK(content_message_id_ != -1);
  size_.set_width(texture_size.width());
  size_.set_height(texture_size.height());

  cc::SkiaPaintCanvas paint_canvas(sk_canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  gfx::Canvas* canvas = &gfx_canvas;

  // Prompt text area.
  auto text = l10n_util::GetStringUTF16(content_message_id_);
  gfx::FontList fonts;
  GetDefaultFontList(ToPixels(kFontSizePromptText), text, &fonts);
  gfx::Rect prompt_text_size(size_.width(), 0);
  std::vector<std::unique_ptr<gfx::RenderText>> lines =
      PrepareDrawStringRect(text, fonts, foreground_color(), &prompt_text_size,
                            kTextAlignmentCenter, kWrappingBehaviorWrap);
  for (auto& render_text : lines)
    render_text->Draw(canvas);

  SkPaint paint;
  gfx::Rect button_text_size(ToPixels(kButtonWidth), 0);
  float radius = size_.width() * kButtonRadiusFactor;

  // Secondary button area.
  // TODO(https://crbug.com/787654): Uppercasing should be conditional.
  text = base::i18n::ToUpper(
      l10n_util::GetStringUTF16(IDS_VR_SHELL_EXIT_PROMPT_EXIT_VR_BUTTON));
  GetDefaultFontList(ToPixels(kFontSizePromptButtonText), text, &fonts);
  lines = PrepareDrawStringRect(
      text, fonts, secondary_button_colors_.foreground, &button_text_size,
      kTextAlignmentCenter, kWrappingBehaviorWrap);
  secondary_button_rect_.SetRect(
      ToPixels(kWidth / 2 - kButtonsSeperatorWidth - kButtonWidth),
      prompt_text_size.height() + ToPixels(kPromptTextButtonSeperatorHeight),
      ToPixels(kButtonWidth), ToPixels(kButtonHeight));
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
  // TODO(https://crbug.com/787654): Uppercasing should be conditional.
  text = base::i18n::ToUpper(l10n_util::GetStringUTF16(IDS_OK));
  GetDefaultFontList(ToPixels(kFontSizePromptButtonText), text, &fonts);
  button_text_size.set_size(gfx::Size(ToPixels(kButtonWidth), 0));
  lines = PrepareDrawStringRect(text, fonts, primary_button_colors_.foreground,
                                &button_text_size, kTextAlignmentCenter,
                                kWrappingBehaviorWrap);
  primary_button_rect_.SetRect(
      ToPixels(kWidth / 2 + kButtonsSeperatorWidth),
      prompt_text_size.height() + ToPixels(kPromptTextButtonSeperatorHeight),
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

float ExitPromptTexture::ToPixels(float meters) const {
  return meters * size_.width() / kWidth;
}

gfx::PointF ExitPromptTexture::PercentToPixels(
    const gfx::PointF& percent) const {
  return gfx::PointF(percent.x() * size_.width(), percent.y() * size_.height());
}

bool ExitPromptTexture::HitsPrimaryButton(const gfx::PointF& position) const {
  return primary_button_rect_.Contains(PercentToPixels(position));
}

bool ExitPromptTexture::HitsSecondaryButton(const gfx::PointF& position) const {
  return secondary_button_rect_.Contains(PercentToPixels(position));
}

void ExitPromptTexture::SetPrimaryButtonHovered(bool hovered) {
  SetAndDirty(&primary_hovered_, hovered);
}

void ExitPromptTexture::SetPrimaryButtonPressed(bool pressed) {
  SetAndDirty(&primary_pressed_, pressed);
}

void ExitPromptTexture::SetSecondaryButtonHovered(bool hovered) {
  SetAndDirty(&secondary_hovered_, hovered);
}

void ExitPromptTexture::SetSecondaryButtonPressed(bool pressed) {
  SetAndDirty(&secondary_pressed_, pressed);
}

void ExitPromptTexture::SetContentMessageId(int message_id) {
  SetAndDirty(&content_message_id_, message_id);
}

void ExitPromptTexture::SetPrimaryButtonColors(const ButtonColors& colors) {
  SetAndDirty(&primary_button_colors_, colors);
}

void ExitPromptTexture::SetSecondaryButtonColors(const ButtonColors& colors) {
  SetAndDirty(&secondary_button_colors_, colors);
}

gfx::Size ExitPromptTexture::GetPreferredTextureSize(int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width * kHeight / kWidth);
}

gfx::SizeF ExitPromptTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr
