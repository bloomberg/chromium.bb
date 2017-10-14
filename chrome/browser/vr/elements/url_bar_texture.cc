// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/url_bar_texture.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/elements/render_text_wrapper.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"

namespace vr {

namespace {

static constexpr float kWidth = 0.672f;
static constexpr float kHeight = 0.088f;
static constexpr float kFontHeight = 0.027f;
static constexpr float kBackButtonWidth = kHeight;
static constexpr float kBackIconSize = 0.0375f;
static constexpr float kBackIconOffset = 0.005f;
static constexpr float kFieldSpacing = 0.014f;
static constexpr float kSecurityIconSize = 0.03f;
static constexpr float kUrlRightMargin = 0.02f;
static constexpr float kSeparatorWidth = 0.002f;
static constexpr float kChipTextLineMargin = kHeight * 0.3f;
static constexpr SkScalar kStrikeThicknessFactor = (SK_Scalar1 / 9);

using security_state::SecurityLevel;

// See LocationBarView::GetSecureTextColor().
SkColor GetSchemeColor(SecurityLevel level, const ColorScheme& color_scheme) {
  switch (level) {
    case SecurityLevel::NONE:
    case SecurityLevel::HTTP_SHOW_WARNING:
      return color_scheme.url_deemphasized;
    case SecurityLevel::EV_SECURE:
    case SecurityLevel::SECURE:
      return color_scheme.secure;
    case SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT:  // ChromeOS only.
      return color_scheme.insecure;
    case SecurityLevel::DANGEROUS:
      return color_scheme.insecure;
    default:
      NOTREACHED();
      return color_scheme.insecure;
  }
}

SkColor GetSecurityChipColor(SecurityLevel level,
                             bool offline_page,
                             const ColorScheme& color_scheme) {
  return offline_page ? color_scheme.offline_page_warning
                      : GetSchemeColor(level, color_scheme);
}

void SetEmphasis(RenderTextWrapper* render_text,
                 bool emphasis,
                 const gfx::Range& range,
                 const ColorScheme& color_scheme) {
  SkColor color =
      emphasis ? color_scheme.url_emphasized : color_scheme.url_deemphasized;
  if (range.IsValid()) {
    render_text->ApplyColor(color, range);
  } else {
    render_text->SetColor(color);
  }
}

gfx::PointF PercentToMeters(const gfx::PointF& percent) {
  return gfx::PointF(percent.x() * kWidth, percent.y() * kHeight);
}

}  // namespace

UrlBarTexture::UrlBarTexture(
    const base::Callback<void(UiUnsupportedMode)>& failure_callback)
    : failure_callback_(failure_callback) {}

UrlBarTexture::~UrlBarTexture() = default;

void UrlBarTexture::SetToolbarState(const ToolbarState& state) {
  if (state_ == state)
    return;
  state_ = state;
  url_dirty_ = true;
  set_dirty();
}

void UrlBarTexture::SetHistoryButtonsEnabled(bool can_go_back) {
  if (can_go_back == can_go_back_)
    return;
  can_go_back_ = can_go_back;
  set_dirty();
}

float UrlBarTexture::ToPixels(float meters) const {
  return meters * size_.width() / kWidth;
}

float UrlBarTexture::ToMeters(float pixels) const {
  return pixels * kWidth / size_.width();
}

bool UrlBarTexture::HitsBackButton(const gfx::PointF& position) const {
  const gfx::PointF& meters = PercentToMeters(position);
  const gfx::RectF region(0, 0, kBackButtonWidth, kHeight);
  return region.Contains(meters) && !HitsTransparentRegion(meters, true);
}

bool UrlBarTexture::HitsUrlBar(const gfx::PointF& position) const {
  const gfx::PointF& meters = PercentToMeters(position);
  gfx::RectF rect(gfx::PointF(kBackButtonWidth, 0),
                  gfx::SizeF(kWidth - kBackButtonWidth, kHeight));
  return rect.Contains(meters) && !HitsTransparentRegion(meters, false);
}

bool UrlBarTexture::HitsSecurityRegion(const gfx::PointF& position) const {
  return security_hit_region_.Contains(PercentToMeters(position));
}

bool UrlBarTexture::HitsTransparentRegion(const gfx::PointF& meters,
                                          bool left) const {
  const float radius = kHeight / 2.0f;
  gfx::PointF circle_center(left ? radius : kWidth - radius, radius);
  if (!left && meters.x() < circle_center.x())
    return false;
  if (left && meters.x() > circle_center.x())
    return false;
  return (meters - circle_center).LengthSquared() > radius * radius;
}

void UrlBarTexture::SetBackButtonHovered(bool hovered) {
  if (back_hovered_ != hovered)
    set_dirty();
  back_hovered_ = hovered;
}

void UrlBarTexture::SetBackButtonPressed(bool pressed) {
  if (back_pressed_ != pressed)
    set_dirty();
  back_pressed_ = pressed;
}

SkColor UrlBarTexture::BackButtonColor() const {
  if (can_go_back_ && back_pressed_)
    return color_scheme().element_background_down;
  if (can_go_back_ && back_hovered_)
    return color_scheme().element_background_hover;
  return color_scheme().element_background;
}

void UrlBarTexture::OnSetMode() {
  url_dirty_ = true;
  set_dirty();
}

void UrlBarTexture::Draw(SkCanvas* canvas, const gfx::Size& texture_size) {
  size_.set_height(texture_size.height());
  size_.set_width(texture_size.width());

  rendered_url_text_ = base::string16();
  rendered_url_text_rect_ = gfx::Rect();
  rendered_security_text_ = base::string16();
  rendered_security_text_rect_ = gfx::Rect();
  security_hit_region_.SetRect(0, 0, 0, 0);

  float height = ToPixels(kHeight);
  float width = ToPixels(kWidth);

  // Make a gfx canvas to support utility drawing methods.
  cc::SkiaPaintCanvas paint_canvas(canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);

  // Back button region.
  SkRRect round_rect;
  SkVector rounded_corner = {height / 2, height / 2};
  SkVector left_corners[4] = {rounded_corner, {0, 0}, {0, 0}, rounded_corner};
  round_rect.setRectRadii({0, 0, height, height}, left_corners);
  SkPaint paint;
  paint.setColor(BackButtonColor());
  canvas->drawRRect(round_rect, paint);

  // Back button icon.
  DrawVectorIcon(
      &gfx_canvas, vector_icons::kBackArrowIcon, ToPixels(kBackIconSize),
      {ToPixels(kBackButtonWidth / 2 + kBackIconOffset - kBackIconSize / 2),
       ToPixels(kHeight - kBackIconSize) / 2},
      can_go_back_ ? color_scheme().element_foreground
                   : color_scheme().disabled);

  // Security indicator and URL area.
  paint.setColor(color_scheme().element_background);
  SkVector right_corners[4] = {{0, 0}, rounded_corner, rounded_corner, {0, 0}};
  round_rect.setRectRadii({height, 0, width, height}, right_corners);
  canvas->drawRRect(round_rect, paint);

  // Back button / URL separator vertical line.
  paint.setColor(color_scheme().separator);
  canvas->drawRect(SkRect::MakeXYWH(ToPixels(kBackButtonWidth), 0,
                                    ToPixels(kSeparatorWidth), height),
                   paint);

  // Keep track of horizontal position as elements are added left to right.
  float left_edge = kBackButtonWidth + kSeparatorWidth + kFieldSpacing;

  // Site security state icon.
  if ((state_.security_level != security_state::NONE || state_.offline_page) &&
      state_.vector_icon != nullptr && state_.should_display_url) {
    gfx::RectF icon_region(left_edge, kHeight / 2 - kSecurityIconSize / 2,
                           kSecurityIconSize, kSecurityIconSize);
    DrawVectorIcon(&gfx_canvas, *state_.vector_icon,
                   ToPixels(kSecurityIconSize),
                   {ToPixels(icon_region.x()), ToPixels(icon_region.y())},
                   GetSecurityChipColor(state_.security_level,
                                        state_.offline_page, color_scheme()));
    security_hit_region_ = icon_region;
    left_edge += kSecurityIconSize + kFieldSpacing;
  }

  // Possibly draw security chip text (eg. "Not secure") next to the icon.
  // The security chip text consumes a significant percentage of URL bar text
  // space, so it is currently disabled (see crbug.com/734206). The offline
  // state is an exception, and must be shown (see crbug.com/735770).
  if (state_.offline_page && state_.should_display_url) {
    float chip_max_width = kWidth - left_edge - kUrlRightMargin;
    gfx::Rect text_bounds(ToPixels(left_edge), 0, ToPixels(chip_max_width),
                          ToPixels(kHeight));

    int pixel_font_height = texture_size.height() * kFontHeight / kHeight;
    SkColor chip_color = GetSecurityChipColor(
        state_.security_level, state_.offline_page, color_scheme());
    const base::string16& chip_text = state_.secure_verbose_text;
    DCHECK(!chip_text.empty());

    gfx::FontList font_list;
    if (!GetFontList(pixel_font_height, chip_text, &font_list))
      failure_callback_.Run(UiUnsupportedMode::kUnhandledCodePoint);

    std::unique_ptr<gfx::RenderText> render_text(CreateRenderText());
    render_text->SetFontList(font_list);
    render_text->SetColor(chip_color);
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
    left_edge += kFieldSpacing;
    paint.setColor(color_scheme().url_deemphasized);
    canvas->drawRect(
        SkRect::MakeXYWH(ToPixels(left_edge), ToPixels(kChipTextLineMargin),
                         ToPixels(kSeparatorWidth),
                         ToPixels(kHeight - 2 * kChipTextLineMargin)),
        paint);
    left_edge += kFieldSpacing + kSeparatorWidth;
  }

  if (state_.should_display_url) {
    float url_x = left_edge;
    if (!url_render_text_ || url_dirty_) {
      float url_width = kWidth - url_x - kUrlRightMargin;
      gfx::Rect text_bounds(ToPixels(url_x), 0, ToPixels(url_width),
                            ToPixels(kHeight));
      RenderUrl(texture_size, text_bounds);
      url_dirty_ = false;
    }
    url_render_text_->Draw(&gfx_canvas);
    rendered_url_text_ = url_render_text_->text();
    rendered_url_text_rect_ = url_render_text_->display_rect();
  }
}

void UrlBarTexture::RenderUrl(const gfx::Size& texture_size,
                              const gfx::Rect& text_bounds) {

  url_formatter::FormatUrlTypes format_types =
      url_formatter::kFormatUrlOmitDefaults;
  if (state_.offline_page)
    format_types |= url_formatter::kFormatUrlOmitHTTPS;

  url::Parsed parsed;
  const base::string16 unelided_url = url_formatter::FormatUrl(
      state_.gurl, format_types, net::UnescapeRule::NORMAL, &parsed, nullptr,
      nullptr);

  int pixel_font_height = texture_size.height() * kFontHeight / kHeight;
  gfx::FontList font_list;
  if (!GetFontList(pixel_font_height, unelided_url, &font_list))
    failure_callback_.Run(UiUnsupportedMode::kUnhandledCodePoint);

  const base::string16 text = url_formatter::ElideUrlSimple(
      state_.gurl, unelided_url, font_list, text_bounds.width(), &parsed);

  std::unique_ptr<gfx::RenderText> render_text(CreateRenderText());
  render_text->SetFontList(font_list);
  render_text->SetColor(SK_ColorBLACK);
  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  render_text->SetElideBehavior(gfx::ELIDE_TAIL);
  render_text->SetDirectionalityMode(gfx::DIRECTIONALITY_FORCE_LTR);
  render_text->SetText(text);
  render_text->SetDisplayRect(text_bounds);

  RenderTextWrapper vr_render_text(render_text.get());
  ApplyUrlStyling(text, parsed, state_.security_level, &vr_render_text,
                  color_scheme());

  url_render_text_ = std::move(render_text);
}

// static
// This method replicates behavior in OmniboxView::UpdateTextStyle(), and
// attempts to maintain similar code structure.
void UrlBarTexture::ApplyUrlStyling(
    const base::string16& formatted_url,
    const url::Parsed& parsed,
    const security_state::SecurityLevel security_level,
    RenderTextWrapper* render_text,
    const ColorScheme& color_scheme) {
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
      SetEmphasis(render_text, false, gfx::Range::InvalidRange(), color_scheme);
      break;
    case NOTHING:
      SetEmphasis(render_text, true, gfx::Range::InvalidRange(), color_scheme);
      break;
    case ALL_BUT_SCHEME:
      DCHECK(scheme_range.IsValid());
      SetEmphasis(render_text, false, gfx::Range::InvalidRange(), color_scheme);
      SetEmphasis(render_text, true, scheme_range, color_scheme);
      break;
    case ALL_BUT_HOST:
      SetEmphasis(render_text, false, gfx::Range::InvalidRange(), color_scheme);
      SetEmphasis(render_text, true, gfx::Range(host.begin, host.end()),
                  color_scheme);
      break;
  }

  // Only SECURE and DANGEROUS levels (pages served over HTTPS or flagged by
  // SafeBrowsing) get a special scheme color treatment. If the security level
  // is NONE or HTTP_SHOW_WARNING, we do not override the text style previously
  // applied to the scheme text range by SetEmphasis().
  if (scheme_range.IsValid() && security_level != security_state::NONE &&
      security_level != security_state::HTTP_SHOW_WARNING) {
    render_text->ApplyColor(GetSchemeColor(security_level, color_scheme),
                            scheme_range);
    if (security_level == SecurityLevel::DANGEROUS) {
      render_text->SetStrikeThicknessFactor(kStrikeThicknessFactor);
      render_text->ApplyStyle(gfx::TextStyle::STRIKE, true, scheme_range);
    }
  }
}

gfx::Size UrlBarTexture::GetPreferredTextureSize(int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width * kHeight / kWidth);
}

gfx::SizeF UrlBarTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr
