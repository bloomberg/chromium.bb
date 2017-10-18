// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/webvr_url_toast_texture.h"

#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

namespace vr {

namespace {

static constexpr float kWidth = 0.472f;
static constexpr float kHeight = 0.064f;
static constexpr float kFontHeight = 0.027f;
static constexpr float kSecurityIconOffsetLeft = 0.022f;
static constexpr float kSecurityIconOffsetRight = 0.016f;
static constexpr float kSecurityIconSize = 0.03f;
static constexpr float kUrlRightMargin = 0.02f;
static constexpr float kRadius = 0.004f;

}  // namespace

WebVrUrlToastTexture::WebVrUrlToastTexture(
    const base::Callback<void(UiUnsupportedMode)>& failure_callback)
    : failure_callback_(failure_callback) {}

WebVrUrlToastTexture::~WebVrUrlToastTexture() = default;

gfx::Size WebVrUrlToastTexture::GetPreferredTextureSize(
    int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width * kHeight / kWidth);
}

gfx::SizeF WebVrUrlToastTexture::GetDrawnSize() const {
  return size_;
}

void WebVrUrlToastTexture::SetToolbarState(const ToolbarState& state) {
  if (state_ == state)
    return;
  state_ = state;
  url_dirty_ = true;
  set_dirty();
}

void WebVrUrlToastTexture::Draw(SkCanvas* canvas,
                                const gfx::Size& texture_size) {
  size_.set_height(texture_size.height());
  size_.set_width(texture_size.width());

  // Draw background.
  SkPaint paint;
  paint.setColor(color_scheme().transient_warning_background);
  paint.setAlpha(255);
  canvas->drawRoundRect(
      SkRect::MakeXYWH(0, 0, ToPixels(kWidth), ToPixels(kHeight)),
      ToPixels(kRadius), ToPixels(kRadius), paint);

  // Make a gfx canvas to support utility drawing methods.
  cc::SkiaPaintCanvas paint_canvas(canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);

  // Site security state icon.
  if ((state_.security_level != security_state::NONE || state_.offline_page) &&
      state_.vector_icon != nullptr && state_.should_display_url) {
    VectorIcon::DrawVectorIcon(&gfx_canvas, *state_.vector_icon,
                               ToPixels(kSecurityIconSize),
                               {ToPixels(kSecurityIconOffsetLeft),
                                ToPixels((kHeight - kSecurityIconSize) / 2)},
                               color_scheme().transient_warning_foreground);
  }

  if (state_.should_display_url) {
    float url_x =
        kSecurityIconOffsetLeft + kSecurityIconSize + kSecurityIconOffsetRight;
    if (!url_render_text_ || url_dirty_) {
      float url_width = kWidth - url_x - kUrlRightMargin;
      gfx::Rect text_bounds(ToPixels(url_x), 0, ToPixels(url_width),
                            ToPixels(kHeight));
      RenderUrl(texture_size, text_bounds);
      url_dirty_ = false;
    }
    url_render_text_->Draw(&gfx_canvas);
  }
}

void WebVrUrlToastTexture::RenderUrl(const gfx::Size& texture_size,
                                     const gfx::Rect& text_bounds) {
  int pixel_font_height = texture_size.height() * kFontHeight / kHeight;

  url::Parsed parsed;
  url_formatter::FormatUrlTypes format_types =
      url_formatter::kFormatUrlOmitDefaults;
  format_types |= url_formatter::kFormatUrlOmitHTTPS;
  const base::string16 formatted_url = url_formatter::FormatUrl(
      state_.gurl, format_types, net::UnescapeRule::NORMAL, &parsed, nullptr,
      nullptr);

  base::string16 url;
  if (parsed.host.is_valid())
    url = formatted_url.substr(parsed.host.begin, parsed.host.len);

  if (parsed.path.is_nonempty() || parsed.query.is_nonempty() ||
      parsed.ref.is_nonempty()) {
    url += gfx::kForwardSlash + base::string16(gfx::kEllipsisUTF16);
  }

  gfx::FontList font_list;
  if (!UiTexture::GetFontList(pixel_font_height, formatted_url, &font_list))
    failure_callback_.Run(UiUnsupportedMode::kUnhandledCodePoint);

  std::unique_ptr<gfx::RenderText> render_text(CreateRenderText());
  render_text->SetDisplayRect(text_bounds);
  render_text->SetFontList(font_list);
  render_text->SetColor(color_scheme().transient_warning_foreground);
  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  render_text->SetElideBehavior(gfx::ELIDE_HEAD);
  render_text->SetDirectionalityMode(gfx::DIRECTIONALITY_FORCE_LTR);
  render_text->SetText(url);

  url_render_text_ = std::move(render_text);
}

float WebVrUrlToastTexture::ToPixels(float meters) const {
  return meters * size_.width() / kWidth;
}

}  // namespace vr
