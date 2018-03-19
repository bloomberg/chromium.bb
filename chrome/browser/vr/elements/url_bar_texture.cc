// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/url_bar_texture.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/render_text_wrapper.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"

namespace vr {

namespace {

// This element renders a collection of features for origin presentation,
// including the security icon, offline chip text and separator, and URL.
// Most of this could be decomposed into sub-elements in a linear layout, if
// linear layout gains the ability to constrain its total size by limiting one
// (or more) of it's children.
constexpr float kWidth = kUrlBarOriginContentWidthDMM;
constexpr float kHeight = kUrlBarHeightDMM;

using security_state::SecurityLevel;

SkColor GetIconColor(SecurityLevel level, const UrlBarColors& colors) {
  switch (level) {
    case SecurityLevel::NONE:
    case SecurityLevel::HTTP_SHOW_WARNING:
    case SecurityLevel::EV_SECURE:
    case SecurityLevel::SECURE:
      return colors.default_icon;
    case SecurityLevel::DANGEROUS:
      return colors.dangerous_icon;
    case SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT:  // ChromeOS only.
    default:
      NOTREACHED();
      return colors.dangerous_icon;
  }
}

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

gfx::PointF PercentToMeters(const gfx::PointF& percent) {
  return gfx::PointF(percent.x() * kWidth, percent.y() * kHeight);
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

bool UrlBarTexture::HitsSecurityRegion(const gfx::PointF& position) const {
  return security_hit_region_.Contains(PercentToMeters(position));
}

void UrlBarTexture::SetColors(const UrlBarColors& colors) {
  SetAndDirty(&colors_, colors);
}

void UrlBarTexture::Draw(SkCanvas* canvas, const gfx::Size& texture_size) {
  size_.set_height(texture_size.height());
  size_.set_width(texture_size.width());

  rendered_url_text_ = base::string16();
  rendered_url_text_rect_ = gfx::Rect();
  rendered_security_text_ = base::string16();
  rendered_security_text_rect_ = gfx::Rect();
  security_hit_region_ = gfx::RectF();

  // Make a gfx canvas to support utility drawing methods.
  cc::SkiaPaintCanvas paint_canvas(canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);

  if (!state_.should_display_url)
    return;

  // Keep track of horizontal position as elements are added left to right.
  float left_edge = 0;

  // Site security state icon.
  if (state_.vector_icon != nullptr) {
    gfx::RectF icon_region(left_edge,
                           kHeight / 2 - kUrlBarButtonIconSizeDMM / 2,
                           kUrlBarButtonIconSizeDMM, kUrlBarButtonIconSizeDMM);
    VectorIcon::DrawVectorIcon(
        &gfx_canvas, *state_.vector_icon, ToPixels(kUrlBarButtonIconSizeDMM),
        {ToPixels(icon_region.x()), ToPixels(icon_region.y())},
        GetIconColor(state_.security_level, colors_));
    security_hit_region_ = icon_region;
    left_edge += kUrlBarButtonIconSizeDMM;
  }

  std::unique_ptr<gfx::RenderText> render_text;
  gfx::FontList font_list;
  int pixel_font_height =
      texture_size.height() * kUrlBarFontHeightDMM / kHeight;

  // Possibly draw security text (eg. "Offline") next to the icon.  This text
  // consumes a significant percentage of URL bar text space, so for now, only
  // Offline mode shows text (see crbug.com/735770).
  if (state_.offline_page) {
    left_edge += kUrlBarOfflineIconTextSpacingDMM;
    float chip_max_width = kWidth - left_edge;
    gfx::Rect text_bounds(ToPixels(left_edge), 0, ToPixels(chip_max_width),
                          ToPixels(kHeight));

    const base::string16& chip_text = state_.secure_verbose_text;
    DCHECK(!chip_text.empty());

    if (!GetDefaultFontList(pixel_font_height, chip_text, &font_list))
      failure_callback_.Run(UiUnsupportedMode::kUnhandledCodePoint);

    render_text = CreateRenderText();
    render_text->SetFontList(font_list);
    render_text->SetColor(colors_.offline_page_warning);
    render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    render_text->SetText(chip_text);
    render_text->SetDisplayRect(text_bounds);
    render_text->Draw(&gfx_canvas);
    rendered_security_text_ = render_text->text();
    rendered_security_text_rect_ = render_text->display_rect();

    // Capture the rendered text region for future hit testing.
    gfx::Size string_size = render_text->GetStringSize();
    gfx::RectF hit_bounds(
        left_edge, kHeight / 2 - ToMeters(string_size.height()) / 2,
        ToMeters(string_size.width()), ToMeters(string_size.height()));
    security_hit_region_.Union(hit_bounds);
    left_edge += ToMeters(string_size.width());

    // Separator line between security text and URL.
    left_edge += kUrlBarFieldSpacingDMM;
    SkPaint paint;
    paint.setColor(colors_.deemphasized);
    canvas->drawRect(
        SkRect::MakeXYWH(
            ToPixels(left_edge), ToPixels(kUrlBarSecuritySeparatorHeightDMM),
            ToPixels(kUrlBarSeparatorWidthDMM),
            ToPixels(kHeight - 2 * kUrlBarSecuritySeparatorHeightDMM)),
        paint);
    left_edge += kUrlBarSeparatorWidthDMM;
  }

  left_edge += kUrlBarFieldSpacingDMM;

  float url_width = kWidth - left_edge;
  gfx::Rect url_text_bounds(ToPixels(left_edge), 0, ToPixels(url_width),
                            ToPixels(kHeight));
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
  ApplyUrlStyling(text, parsed, state_.security_level, &vr_render_text,
                  colors_);

  ElisionParameters elision_parameters =
      GetElisionParameters(state_.gurl, parsed, render_text.get(),
                           ToPixels(kUrlBarOriginMinimumPathWidth));
  render_text->SetDisplayOffset(elision_parameters.offset);

  render_text->Draw(&gfx_canvas);
  float fade_width = ToPixels(kUrlBarOriginFadeWidth);
  ApplyUrlFading(canvas, render_text->display_rect(), fade_width,
                 elision_parameters.fade_left, elision_parameters.fade_right);

  rendered_url_text_ = render_text->text();
  rendered_url_text_rect_ = render_text->display_rect();
}

// static
// This method replicates behavior in OmniboxView::UpdateTextStyle().
void UrlBarTexture::ApplyUrlStyling(
    const base::string16& formatted_url,
    const url::Parsed& parsed,
    const security_state::SecurityLevel security_level,
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
