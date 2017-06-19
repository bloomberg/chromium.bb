// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/url_bar_texture.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/android/vr_shell/color_scheme.h"
#include "chrome/browser/android/vr_shell/textures/render_text_wrapper.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/vector_icons.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/vector_icons/vector_icons.h"

namespace vr_shell {

namespace {

static constexpr float kWidth = 0.672;
static constexpr float kHeight = 0.088;
static constexpr float kFontHeight = 0.027;
static constexpr float kBackButtonWidth = kHeight;
static constexpr float kBackIconHeight = 0.0375;
static constexpr float kBackIconOffset = 0.005;
static constexpr float kFieldSpacing = 0.014;
static constexpr float kSecurityIconHeight = 0.03;
static constexpr float kUrlRightMargin = 0.02;
static constexpr float kSeparatorWidth = 0.002;
static constexpr float kChipTextLineMargin = kHeight * 0.3;

using security_state::SecurityLevel;

// See ToolbarModelImpl::GetVectorIcon().
const struct gfx::VectorIcon& GetSecurityIcon(SecurityLevel level) {
  switch (level) {
    case security_state::NONE:
    case security_state::HTTP_SHOW_WARNING:
      return toolbar::kHttpIcon;
    case security_state::EV_SECURE:
    case security_state::SECURE:
      return toolbar::kHttpsValidIcon;
    case security_state::SECURITY_WARNING:
      // Surface Dubious as Neutral.
      return toolbar::kHttpIcon;
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:  // ChromeOS only.
      return ui::kBusinessIcon;
    case security_state::DANGEROUS:
      return toolbar::kHttpsInvalidIcon;
    default:
      NOTREACHED();
      return toolbar::kHttpsInvalidIcon;
  }
}

// See LocationBarView::GetSecureTextColor().
SkColor GetSchemeColor(SecurityLevel level, const ColorScheme& color_scheme) {
  switch (level) {
    case SecurityLevel::NONE:
    case SecurityLevel::HTTP_SHOW_WARNING:
      return color_scheme.url_deemphasized;
    case SecurityLevel::EV_SECURE:
    case SecurityLevel::SECURE:
      return color_scheme.secure;
    case SecurityLevel::SECURITY_WARNING:
      return color_scheme.url_deemphasized;
    case SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT:  // ChromeOS only.
      return color_scheme.insecure;
    case SecurityLevel::DANGEROUS:
      return color_scheme.insecure;
    default:
      NOTREACHED();
      return color_scheme.insecure;
  }
}

// See ToolbarModelImpl::GetSecureVerboseText().
int GetSecurityTextId(SecurityLevel level, bool malware) {
  switch (level) {
    case security_state::HTTP_SHOW_WARNING:
      return IDS_NOT_SECURE_VERBOSE_STATE;
    case security_state::SECURE:
      return IDS_SECURE_VERBOSE_STATE;
    case security_state::DANGEROUS:
      return (malware ? IDS_DANGEROUS_VERBOSE_STATE
                      : IDS_NOT_SECURE_VERBOSE_STATE);
    default:
      return 0;
  }
}

void setEmphasis(vr_shell::RenderTextWrapper* render_text,
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

gfx::PointF percentToMeters(const gfx::PointF& percent) {
  return gfx::PointF(percent.x() * kWidth, percent.y() * kHeight);
}

}  // namespace

UrlBarTexture::UrlBarTexture(
    bool web_vr,
    const base::Callback<void(UiUnsupportedMode)>& failure_callback)
    : security_level_(SecurityLevel::DANGEROUS),
      has_back_button_(!web_vr),
      has_security_chip_(false),
      failure_callback_(failure_callback) {}

UrlBarTexture::~UrlBarTexture() = default;

void UrlBarTexture::SetURL(const GURL& gurl) {
  if (gurl_ != gurl)
    set_dirty();
  gurl_ = gurl;
}

void UrlBarTexture::SetHistoryButtonsEnabled(bool can_go_back) {
  if (can_go_back != can_go_back_)
    set_dirty();
  can_go_back_ = can_go_back;
}

void UrlBarTexture::SetSecurityInfo(SecurityLevel level, bool malware) {
  if (security_level_ != level || malware_ != malware)
    set_dirty();
  security_level_ = level;
  malware_ = malware;
}

float UrlBarTexture::ToPixels(float meters) const {
  return meters * size_.width() / kWidth;
}

float UrlBarTexture::ToMeters(float pixels) const {
  return pixels * kWidth / size_.width();
}

bool UrlBarTexture::HitsBackButton(const gfx::PointF& position) const {
  const gfx::PointF& meters = percentToMeters(position);
  return back_button_hit_region_.Contains(meters) &&
         !HitsTransparentRegion(meters, true);
}

bool UrlBarTexture::HitsUrlBar(const gfx::PointF& position) const {
  const gfx::PointF& meters = percentToMeters(position);
  gfx::RectF rect(gfx::PointF(kBackButtonWidth, 0),
                  gfx::SizeF(kWidth - kBackButtonWidth, kHeight));
  return rect.Contains(meters) && !HitsTransparentRegion(meters, false);
}

bool UrlBarTexture::HitsSecurityRegion(const gfx::PointF& position) const {
  return security_hit_region_.Contains(percentToMeters(position));
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

SkColor UrlBarTexture::GetLeftCornerColor() const {
  SkColor color = color_scheme().element_background;
  if (has_back_button_ && can_go_back_) {
    if (back_pressed_)
      color = color_scheme().element_background_down;
    else if (back_hovered_)
      color = color_scheme().element_background_hover;
  }
  return color;
}

void UrlBarTexture::OnSetMode() {
  set_dirty();
}

void UrlBarTexture::Draw(SkCanvas* canvas, const gfx::Size& texture_size) {
  size_.set_height(texture_size.height());
  size_.set_width(texture_size.width());

  canvas->save();
  canvas->scale(size_.width() / kWidth, size_.width() / kWidth);

  // Make a gfx canvas to support utility drawing methods.
  cc::SkiaPaintCanvas paint_canvas(canvas);
  gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);

  // Left rounded corner of URL bar.
  SkRRect round_rect;
  SkVector rounded_corner = {kHeight / 2, kHeight / 2};
  SkVector left_corners[4] = {rounded_corner, {0, 0}, {0, 0}, rounded_corner};
  round_rect.setRectRadii({0, 0, kHeight, kHeight}, left_corners);
  SkPaint paint;
  paint.setColor(GetLeftCornerColor());
  canvas->drawRRect(round_rect, paint);

  // URL area.
  paint.setColor(color_scheme().element_background);
  SkVector right_corners[4] = {{0, 0}, rounded_corner, rounded_corner, {0, 0}};
  round_rect.setRectRadii({kHeight, 0, kWidth, kHeight}, right_corners);
  canvas->drawRRect(round_rect, paint);

  back_button_hit_region_.SetRect(0, 0, 0, 0);
  security_hit_region_.SetRect(0, 0, 0, 0);

  // Keep track of a left edge as we selectively render components of the URL
  // bar left-to-right.
  float left_edge = 0;

  if (has_back_button_) {
    // Back button / URL separator vertical line.
    paint.setColor(color_scheme().separator);
    canvas->drawRect(
        SkRect::MakeXYWH(kBackButtonWidth, 0, kSeparatorWidth, kHeight), paint);

    // Back button icon.
    canvas->save();
    canvas->translate(kBackButtonWidth / 2 + kBackIconOffset, kHeight / 2);
    canvas->translate(-kBackIconHeight / 2, -kBackIconHeight / 2);
    float icon_scale =
        kBackIconHeight / GetDefaultSizeOfVectorIcon(ui::kBackArrowIcon);
    canvas->scale(icon_scale, icon_scale);
    PaintVectorIcon(&gfx_canvas, ui::kBackArrowIcon,
                    can_go_back_ ? color_scheme().element_foreground
                                 : color_scheme().disabled);
    canvas->restore();

    back_button_hit_region_.SetRect(left_edge, 0, left_edge + kBackButtonWidth,
                                    kHeight);
    left_edge += kBackButtonWidth + kSeparatorWidth;
  }

  // Site security state icon.
  if (!gurl_.is_empty()) {
    left_edge += kFieldSpacing;

    gfx::RectF icon_region(left_edge, kHeight / 2 - kSecurityIconHeight / 2,
                           kSecurityIconHeight, kSecurityIconHeight);
    canvas->save();
    canvas->translate(icon_region.x(), icon_region.y());
    const gfx::VectorIcon& icon = GetSecurityIcon(security_level_);
    float icon_scale = kSecurityIconHeight / GetDefaultSizeOfVectorIcon(icon);
    canvas->scale(icon_scale, icon_scale);
    PaintVectorIcon(&gfx_canvas, icon,
                    GetSchemeColor(security_level_, color_scheme()));
    canvas->restore();

    security_hit_region_ = icon_region;
    left_edge += kSecurityIconHeight + kFieldSpacing;
  }

  canvas->restore();

  // Draw security chip text (eg. "Not secure") next to the security icon.
  int chip_string_id = GetSecurityTextId(security_level_, malware_);
  if (has_security_chip_ && !gurl_.is_empty() && chip_string_id != 0) {
    float chip_max_width = kWidth - left_edge - kUrlRightMargin;
    gfx::Rect text_bounds(ToPixels(left_edge), 0, ToPixels(chip_max_width),
                          ToPixels(kHeight));

    int pixel_font_height = texture_size.height() * kFontHeight / kHeight;
    SkColor chip_color = GetSchemeColor(security_level_, color_scheme());
    auto chip_text = l10n_util::GetStringUTF16(chip_string_id);
    DCHECK(!chip_text.empty());

    gfx::FontList font_list;
    if (!GetFontList(pixel_font_height, chip_text, &font_list))
      failure_callback_.Run(UiUnsupportedMode::kUnhandledCodePoint);

    std::unique_ptr<gfx::RenderText> render_text(
        gfx::RenderText::CreateInstance());
    render_text->SetFontList(font_list);
    render_text->SetColor(chip_color);
    render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    render_text->SetText(chip_text);
    render_text->SetDisplayRect(text_bounds);
    render_text->Draw(&gfx_canvas);

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

  if (!gurl_.is_empty()) {
    if (last_drawn_gurl_ != gurl_ ||
        last_drawn_security_level_ != security_level_) {
      float url_x = left_edge;
      float url_width = kWidth - url_x - kUrlRightMargin;
      gfx::Rect text_bounds(ToPixels(url_x), 0, ToPixels(url_width),
                            ToPixels(kHeight));
      RenderUrl(texture_size, text_bounds);
      last_drawn_gurl_ = gurl_;
      last_drawn_security_level_ = security_level_;
    }
    url_render_text_->Draw(&gfx_canvas);
  }
}

void UrlBarTexture::RenderUrl(const gfx::Size& texture_size,
                              const gfx::Rect& bounds) {
  url::Parsed parsed;
  const base::string16 text = url_formatter::FormatUrl(
      gurl_, url_formatter::kFormatUrlOmitAll, net::UnescapeRule::NORMAL,
      &parsed, nullptr, nullptr);

  if (base::i18n::StringContainsStrongRTLChars(text))
    failure_callback_.Run(UiUnsupportedMode::kURLWithStrongRTLChars);

  int pixel_font_height = texture_size.height() * kFontHeight / kHeight;

  gfx::FontList font_list;
  if (!GetFontList(pixel_font_height, text, &font_list))
    failure_callback_.Run(UiUnsupportedMode::kUnhandledCodePoint);

  std::unique_ptr<gfx::RenderText> render_text(
      gfx::RenderText::CreateInstance());
  render_text->SetFontList(font_list);
  render_text->SetColor(SK_ColorBLACK);
  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  render_text->SetElideBehavior(gfx::TRUNCATE);

  base::string16 mandatory_prefix = text;
  int length = parsed.CountCharactersBefore(url::Parsed::PORT, false);
  if (length > 0)
    mandatory_prefix = text.substr(0, length);
  gfx::Rect expanded_bounds = bounds;
  expanded_bounds.set_width(2 * bounds.width());
  render_text->SetDisplayRect(expanded_bounds);
  render_text->SetText(mandatory_prefix);

  if (render_text->GetStringSize().width() > bounds.width()) {
    failure_callback_.Run(UiUnsupportedMode::kCouldNotElideURL);
  }

  render_text->SetText(text);
  render_text->SetDisplayRect(bounds);
  vr_shell::RenderTextWrapper vr_render_text(render_text.get());
  ApplyUrlStyling(text, parsed, security_level_, &vr_render_text,
                  color_scheme());

  url_render_text_ = std::move(render_text);
}

// This method replicates behavior in OmniboxView::UpdateTextStyle(), and
// attempts to maintain similar code structure.
void UrlBarTexture::ApplyUrlStyling(
    const base::string16& formatted_url,
    const url::Parsed& parsed,
    const security_state::SecurityLevel security_level,
    vr_shell::RenderTextWrapper* render_text,
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
      setEmphasis(render_text, false, gfx::Range::InvalidRange(), color_scheme);
      break;
    case NOTHING:
      setEmphasis(render_text, true, gfx::Range::InvalidRange(), color_scheme);
      break;
    case ALL_BUT_SCHEME:
      DCHECK(scheme_range.IsValid());
      setEmphasis(render_text, false, gfx::Range::InvalidRange(), color_scheme);
      setEmphasis(render_text, true, scheme_range, color_scheme);
      break;
    case ALL_BUT_HOST:
      setEmphasis(render_text, false, gfx::Range::InvalidRange(), color_scheme);
      setEmphasis(render_text, true, gfx::Range(host.begin, host.end()),
                  color_scheme);
      break;
  }

  // Only SECURE and DANGEROUS levels (pages served over HTTPS or flagged by
  // SafeBrowsing) get a special scheme color treatment. If the security level
  // is NONE or HTTP_SHOW_WARNING, we do not override the text style previously
  // applied to the scheme text range by setEmphasis().
  if (scheme_range.IsValid() && security_level != security_state::NONE &&
      security_level != security_state::HTTP_SHOW_WARNING) {
    render_text->ApplyColor(GetSchemeColor(security_level, color_scheme),
                            scheme_range);
    if (security_level == SecurityLevel::DANGEROUS) {
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

}  // namespace vr_shell
