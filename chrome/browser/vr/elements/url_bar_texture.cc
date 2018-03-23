// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/url_bar_texture.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/render_text_wrapper.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"

namespace vr {

namespace {

// This element renders a collection of features for origin presentation,
// including the security icon, offline chip text and separator, and URL.
// Most of this could be decomposed into sub-elements in a linear layout, if
// linear layout gains the ability to constrain its total size by limiting one
// (or more) of it's children.
constexpr float kWidth = kUrlBarUrlWidthDMM;
constexpr float kHeight = kUrlBarHeightDMM;

void SetEmphasis(RenderTextWrapper* render_text,
                 bool emphasis,
                 const gfx::Range& range,
                 const UrlBarColors& colors) {
  SkColor color = emphasis ? colors.emphasized : colors.deemphasized;
  if (range.IsValid()) {
    render_text->ApplyColor(color, range);
  } else {
    render_text->SetColor(color);
  }
}

void ApplyUrlFading(SkCanvas* canvas,
                    const gfx::Rect& text_bounds,
                    float fade_width,
                    bool fade_left,
                    bool fade_right) {
  if (!fade_left && !fade_right)
    return;

  SkPoint fade_points[2] = {SkPoint::Make(0.0f, 0.0f),
                            SkPoint::Make(fade_width, 0.0f)};
  SkColor fade_colors[2] = {SK_ColorTRANSPARENT, SK_ColorBLACK};

  SkPaint overlay;
  overlay.setShader(
      SkGradientShader::MakeLinear(fade_points, fade_colors, nullptr, 2,
                                   SkShader::kClamp_TileMode, 0, nullptr));
  if (fade_left) {
    canvas->save();
    canvas->translate(text_bounds.x(), 0);
    canvas->clipRect(SkRect::MakeWH(fade_width, text_bounds.height()));
    overlay.setBlendMode(SkBlendMode::kDstIn);
    canvas->drawPaint(overlay);
    canvas->restore();
  }

  if (fade_right) {
    canvas->save();
    canvas->translate(text_bounds.right() - fade_width, 0);
    canvas->clipRect(SkRect::MakeWH(fade_width, text_bounds.height()));
    overlay.setBlendMode(SkBlendMode::kDstOut);
    canvas->drawPaint(overlay);
    canvas->restore();
  }
}

}  // namespace

UrlBarTexture::UrlBarTexture(
    const base::Callback<void(UiUnsupportedMode)>& failure_callback)
    : failure_callback_(failure_callback) {}

UrlBarTexture::~UrlBarTexture() = default;

void UrlBarTexture::SetToolbarState(const ToolbarState& state) {
  SetAndDirty(&state_, state);
}

float UrlBarTexture::ToPixels(float meters) const {
  return meters * size_.width() / kWidth;
}

float UrlBarTexture::ToMeters(float pixels) const {
  return pixels * kWidth / size_.width();
}

void UrlBarTexture::SetColors(const UrlBarColors& colors) {
  SetAndDirty(&colors_, colors);
}

void UrlBarTexture::Draw(SkCanvas* canvas, const gfx::Size& texture_size) {
  size_.set_height(texture_size.height());
  size_.set_width(texture_size.width());

  std::unique_ptr<gfx::RenderText> render_text;
  gfx::FontList font_list;
  int pixel_font_height =
      texture_size.height() * kUrlBarFontHeightDMM / kHeight;

  gfx::Rect url_text_bounds(0, 0, texture_size.width(), texture_size.height());

  url::Parsed parsed;
  const base::string16 text = url_formatter::FormatUrl(
      state_.gurl, GetVrFormatUrlTypes(), net::UnescapeRule::NORMAL, &parsed,
      nullptr, nullptr);

  if (!GetDefaultFontList(pixel_font_height, text, &font_list))
    failure_callback_.Run(UiUnsupportedMode::kUnhandledCodePoint);

  render_text = CreateRenderText();
  render_text->SetFontList(font_list);
  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  render_text->SetDirectionalityMode(gfx::DIRECTIONALITY_AS_URL);
  render_text->SetText(text);
  render_text->SetDisplayRect(url_text_bounds);

  RenderTextWrapper vr_render_text(render_text.get());
  ApplyUrlStyling(text, parsed, &vr_render_text, colors_);

  ElisionParameters elision_parameters =
      GetElisionParameters(state_.gurl, parsed, render_text.get(),
                           ToPixels(kUrlBarOriginMinimumPathWidth));
  render_text->SetDisplayOffset(elision_parameters.offset);

  cc::SkiaPaintCanvas paint_canvas(canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);
  render_text->Draw(&gfx_canvas);

  float fade_width = ToPixels(kUrlBarOriginFadeWidth);
  ApplyUrlFading(canvas, render_text->display_rect(), fade_width,
                 elision_parameters.fade_left, elision_parameters.fade_right);
}

// static
// This method replicates behavior in OmniboxView::UpdateTextStyle().
void UrlBarTexture::ApplyUrlStyling(
    const base::string16& formatted_url,
    const url::Parsed& parsed,
    RenderTextWrapper* render_text,
    const UrlBarColors& colors) {
  const url::Component& scheme = parsed.scheme;
  const url::Component& host = parsed.host;

  enum DeemphasizeComponents {
    EVERYTHING,
    ALL_BUT_SCHEME,
    ALL_BUT_HOST,
    NOTHING,
  } deemphasize = NOTHING;

  const base::string16 url_scheme =
      formatted_url.substr(scheme.begin, scheme.len);

  // Data URLs are rarely human-readable and can be used for spoofing, so draw
  // attention to the scheme to emphasize "this is just a bunch of data".  For
  // normal URLs, the host is the best proxy for "identity".
  // TODO(cjgrant): Handle extensions, if required, for desktop.
  if (url_scheme == base::UTF8ToUTF16(url::kDataScheme))
    deemphasize = ALL_BUT_SCHEME;
  else if (host.is_nonempty())
    deemphasize = ALL_BUT_HOST;

  gfx::Range scheme_range = scheme.is_nonempty()
                                ? gfx::Range(scheme.begin, scheme.end())
                                : gfx::Range::InvalidRange();
  switch (deemphasize) {
    case EVERYTHING:
      SetEmphasis(render_text, false, gfx::Range::InvalidRange(), colors);
      break;
    case NOTHING:
      SetEmphasis(render_text, true, gfx::Range::InvalidRange(), colors);
      break;
    case ALL_BUT_SCHEME:
      DCHECK(scheme_range.IsValid());
      SetEmphasis(render_text, false, gfx::Range::InvalidRange(), colors);
      SetEmphasis(render_text, true, scheme_range, colors);
      break;
    case ALL_BUT_HOST:
      SetEmphasis(render_text, false, gfx::Range::InvalidRange(), colors);
      SetEmphasis(render_text, true, gfx::Range(host.begin, host.end()),
                  colors);
      break;
  }
}

gfx::Size UrlBarTexture::GetPreferredTextureSize(int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width * kHeight / kWidth);
}

gfx::SizeF UrlBarTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr
