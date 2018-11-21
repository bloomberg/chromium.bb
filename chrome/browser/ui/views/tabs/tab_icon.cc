// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_icon.h"

#include "cc/paint/paint_flags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace {

// Note: The excess (part above 1.0) is used as a fade-out effect, so if this
// value is 1.5, then 50% of the progress indicator animation will be used for
// fade-out time after the loading sequence is complete.
// Also note that the progress indicator animation is capped to the actual load
// progress.
constexpr double kProgressIndicatorAnimationDuration = 1.5;

bool UseNewLoadingAnimation() {
  return base::FeatureList::IsEnabled(features::kNewTabLoadingAnimation);
}

constexpr int kAttentionIndicatorRadius = 3;

// Returns whether the favicon for the given URL should be colored according to
// the browser theme.
bool ShouldThemifyFaviconForUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() != chrome::kChromeUIHelpHost &&
         url.host_piece() != chrome::kChromeUIUberHost &&
         url.host_piece() != chrome::kChromeUIAppLauncherPageHost;
}

// Returns a rect in which the throbber should be painted.
gfx::RectF GetThrobberBounds(const gfx::Rect& bounds) {
  gfx::RectF throbber_bounds(bounds);
  constexpr float kThrobberHeightDp = 2;
  // The throbber starts 1dp below the tab icon.
  throbber_bounds.set_y(bounds.bottom() + 1);
  throbber_bounds.set_height(kThrobberHeightDp);
  return throbber_bounds;
}

}  // namespace

// Helper class that manages the favicon crash animation.
class TabIcon::CrashAnimation : public gfx::LinearAnimation,
                                public gfx::AnimationDelegate {
 public:
  explicit CrashAnimation(TabIcon* target)
      : gfx::LinearAnimation(base::TimeDelta::FromSeconds(1), 25, this),
        target_(target) {}
  ~CrashAnimation() override = default;

  // gfx::Animation overrides:
  void AnimateToState(double state) override {
    if (state < .5) {
      // Animate the normal icon down.
      target_->hiding_fraction_ = state * 2.0;
    } else {
      // Animate the crashed icon up.
      target_->should_display_crashed_favicon_ = true;
      target_->hiding_fraction_ = 1.0 - (state - 0.5) * 2.0;
    }
    target_->SchedulePaint();
  }

 private:
  TabIcon* target_;

  DISALLOW_COPY_AND_ASSIGN(CrashAnimation);
};

TabIcon::TabIcon()
    : loading_progress_timer_(base::TimeDelta::FromMilliseconds(600),
                              gfx::LinearAnimation::kDefaultFrameRate,
                              this),
      favicon_fade_in_animation_(base::TimeDelta::FromMilliseconds(200),
                                 gfx::LinearAnimation::kDefaultFrameRate,
                                 this) {
  set_can_process_events_within_subtree(false);

  // The minimum size to avoid clipping the attention indicator.
  SetPreferredSize(gfx::Size(gfx::kFaviconSize + kAttentionIndicatorRadius,
                             gfx::kFaviconSize + kAttentionIndicatorRadius));
}

TabIcon::~TabIcon() = default;

void TabIcon::SetData(const TabRendererData& data) {
  const bool was_showing_load = ShowingLoadingAnimation();

  inhibit_loading_animation_ = data.should_hide_throbber;
  SetIcon(data.url, data.favicon);
  SetNetworkState(data.network_state, data.load_progress);
  SetIsCrashed(data.IsCrashed());

  const bool showing_load = ShowingLoadingAnimation();

  RefreshLayer();

  if (was_showing_load && !showing_load) {
    // Loading animation transitioning from on to off.
    loading_start_time_ = base::TimeTicks();
    waiting_state_ = gfx::ThrobberWaitingState();
    SchedulePaint();
  } else if (!was_showing_load && showing_load) {
    // Loading animation transitioning from off to on. The animation painting
    // function will lazily initialize the data.
    SchedulePaint();
  }
}

void TabIcon::SetAttention(AttentionType type, bool enabled) {
  int previous_attention_type = attention_types_;
  if (enabled)
    attention_types_ |= static_cast<int>(type);
  else
    attention_types_ &= ~static_cast<int>(type);

  if (attention_types_ != previous_attention_type)
    SchedulePaint();
}

bool TabIcon::ShowingLoadingAnimation() const {
  if (inhibit_loading_animation_)
    return false;

  if (loading_progress_timer_.is_animating() ||
      favicon_fade_in_animation_.is_animating()) {
    return true;
  }

  return network_state_ != TabNetworkState::kNone &&
         network_state_ != TabNetworkState::kError;
}

bool TabIcon::ShowingAttentionIndicator() const {
  return attention_types_ > 0;
}

void TabIcon::SetCanPaintToLayer(bool can_paint_to_layer) {
  if (can_paint_to_layer == can_paint_to_layer_)
    return;
  can_paint_to_layer_ = can_paint_to_layer;
  RefreshLayer();
}

void TabIcon::StepLoadingAnimation(const base::TimeDelta& elapsed_time) {
  waiting_state_.elapsed_time = elapsed_time;
  if (ShowingLoadingAnimation())
    SchedulePaint();
}

void TabIcon::SetBackgroundColor(SkColor bg_color) {
  bg_color_ = bg_color;
  SchedulePaint();
}

void TabIcon::OnPaint(gfx::Canvas* canvas) {
  // Compute the bounds adjusted for the hiding fraction.
  gfx::Rect contents_bounds = GetContentsBounds();
  if (contents_bounds.IsEmpty())
    return;
  gfx::Rect icon_bounds(
      GetMirroredXWithWidthInView(0, gfx::kFaviconSize),
      static_cast<int>(contents_bounds.height() * hiding_fraction_),
      std::min(gfx::kFaviconSize, contents_bounds.width()),
      std::min(gfx::kFaviconSize, contents_bounds.height()));

  // The old animation replaces the favicon and should early-abort.
  if (!UseNewLoadingAnimation() && ShowingLoadingAnimation()) {
    PaintLoadingAnimation(canvas, icon_bounds);
    return;
  }

  if (ShowingAttentionIndicator() && !should_display_crashed_favicon_) {
    PaintAttentionIndicatorAndIcon(canvas, GetIconToPaint(), icon_bounds);
  } else if (!MaybePaintFavicon(canvas, GetIconToPaint(), icon_bounds)) {
    PaintFaviconPlaceholder(canvas, icon_bounds);
  }

  if (ShowingLoadingAnimation())
    PaintLoadingAnimation(canvas, icon_bounds);
}

void TabIcon::OnThemeChanged() {
  crashed_icon_ = gfx::ImageSkia();  // Force recomputation if crashed.
  if (!themed_favicon_.isNull())
    themed_favicon_ = ThemeImage(favicon_);
}

void TabIcon::AnimationEnded(const gfx::Animation* animation) {
  // After the last animation ends it's possible we should not paint to a layer
  // anymore.
  RefreshLayer();
  // Make sure that the final frame of the animation gets painted.
  SchedulePaint();
}

void TabIcon::PaintAttentionIndicatorAndIcon(gfx::Canvas* canvas,
                                             const gfx::ImageSkia& icon,
                                             const gfx::Rect& bounds) {
  gfx::Point circle_center(
      bounds.x() + (base::i18n::IsRTL() ? 0 : gfx::kFaviconSize),
      bounds.y() + gfx::kFaviconSize);

  // The attention indicator consists of two parts:
  // . a clear (totally transparent) part over the bottom right (or left in rtl)
  //   of the favicon. This is done by drawing the favicon to a layer, then
  //   drawing the clear part on top of the favicon.
  // . a circle in the bottom right (or left in rtl) of the favicon.
  if (!icon.isNull()) {
    canvas->SaveLayerAlpha(0xff);
    canvas->DrawImageInt(icon, 0, 0, bounds.width(), bounds.height(),
                         bounds.x(), bounds.y(), bounds.width(),
                         bounds.height(), false);
    cc::PaintFlags clear_flags;
    clear_flags.setAntiAlias(true);
    clear_flags.setBlendMode(SkBlendMode::kClear);
    const float kIndicatorCropRadius = 4.5f;
    canvas->DrawCircle(circle_center, kIndicatorCropRadius, clear_flags);
    canvas->Restore();
  }

  // Draws the actual attention indicator.
  cc::PaintFlags indicator_flags;
  indicator_flags.setColor(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor));
  indicator_flags.setAntiAlias(true);
  canvas->DrawCircle(circle_center, kAttentionIndicatorRadius, indicator_flags);
}

void TabIcon::PaintLoadingProgressIndicator(gfx::Canvas* canvas,
                                            gfx::RectF bounds,
                                            SkColor color) {
  double animation_value = loading_progress_timer_.GetCurrentValue() *
                           kProgressIndicatorAnimationDuration;
  if (network_state_ == TabNetworkState::kLoading) {
    // While we're loading, clamp to the loading process (which cannot go above
    // 1.0) to make sure that we don't start the fade-out during kLoading, even
    // if loading progress has previously been reported as 1.0.
    animation_value = std::min(loading_progress_, animation_value);
  }
  const double loading_progress = std::min(animation_value, 1.0);

  bounds.set_width(bounds.height() +
                   loading_progress * (bounds.width() - bounds.height()));

  cc::PaintFlags flags;
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  if (animation_value > 1.0) {
    // The part of kProgressIndicatorAnimationDuration above 1.0 is translated
    // into an alpha value used as a linear fade-out effect.
    flags.setAlpha(SK_AlphaOPAQUE *
                   (1.0 - (animation_value - 1.0) /
                              (kProgressIndicatorAnimationDuration - 1.0)));
  }

  canvas->DrawRect(bounds, flags);
}

void TabIcon::PaintLoadingAnimation(gfx::Canvas* canvas,
                                    const gfx::Rect& bounds) {
  const ui::ThemeProvider* tp = GetThemeProvider();
  if (UseNewLoadingAnimation()) {
    const gfx::RectF throbber_bounds = GetThrobberBounds(bounds);
    // Note that this tab-loading animation intentionally uses
    // COLOR_TAB_THROBBER_SPINNING for both the waiting and loading states.
    const SkColor loading_color =
        tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_SPINNING);
    if (network_state_ == TabNetworkState::kWaiting) {
      gfx::PaintNewThrobberWaiting(canvas, throbber_bounds, loading_color,
                                   waiting_state_.elapsed_time);
    } else {
      PaintLoadingProgressIndicator(canvas, throbber_bounds, loading_color);
    }
  } else {
    if (network_state_ == TabNetworkState::kWaiting) {
      gfx::PaintThrobberWaiting(
          canvas, bounds,
          tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_WAITING),
          waiting_state_.elapsed_time);
    } else {
      const base::TimeTicks current_time = base::TimeTicks::Now();
      if (loading_start_time_ == base::TimeTicks())
        loading_start_time_ = current_time;

      waiting_state_.color =
          tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_WAITING);
      gfx::PaintThrobberSpinningAfterWaiting(
          canvas, bounds,
          tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_SPINNING),
          current_time - loading_start_time_, &waiting_state_);
    }
  }
}

const gfx::ImageSkia& TabIcon::GetIconToPaint() {
  if (should_display_crashed_favicon_) {
    if (crashed_icon_.isNull()) {
      // Lazily create a themed sad tab icon.
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      crashed_icon_ = ThemeImage(*rb.GetImageSkiaNamed(IDR_CRASH_SAD_FAVICON));
    }
    return crashed_icon_;
  }
  return themed_favicon_.isNull() ? favicon_ : themed_favicon_;
}

void TabIcon::PaintFaviconPlaceholder(gfx::Canvas* canvas,
                                      const gfx::Rect& bounds) {
  cc::PaintFlags flags;
  flags.setColor(SkColorSetA(
      color_utils::IsDark(bg_color_) ? SK_ColorWHITE : SK_ColorBLACK, 32));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  constexpr float kFaviconPlaceholderRadiusDp = 4;
  canvas->DrawRoundRect(bounds, kFaviconPlaceholderRadiusDp, flags);
}

bool TabIcon::MaybePaintFavicon(gfx::Canvas* canvas,
                                const gfx::ImageSkia& icon,
                                const gfx::Rect& bounds) {
  // While loading, the favicon (or placeholder) isn't drawn until it has
  // started fading in.
  if (ShowingLoadingAnimation() &&
      favicon_fade_in_animation_.GetCurrentValue() == 0.0) {
    return false;
  }

  if (icon.isNull())
    return false;

  cc::PaintFlags flags;
  // If we're loading and the favicon is fading in, draw with transparency.
  flags.setAlpha(ShowingLoadingAnimation()
                     ? favicon_fade_in_animation_.GetCurrentValue() *
                           SK_AlphaOPAQUE
                     : SK_AlphaOPAQUE);

  canvas->DrawImageInt(icon, 0, 0, bounds.width(), bounds.height(), bounds.x(),
                       bounds.y(), bounds.width(), bounds.height(), false,
                       flags);
  return true;
}

void TabIcon::SetIcon(const GURL& url, const gfx::ImageSkia& icon) {
  // Detect when updating to the same icon. This avoids re-theming and
  // re-painting.
  if (favicon_.BackedBySameObjectAs(icon))
    return;
  favicon_ = icon;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const bool is_default_favicon =
      icon.BackedBySameObjectAs(*rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON));
  if (favicon_fade_in_animation_.GetCurrentValue() == 0.0 &&
      !is_default_favicon) {
    favicon_fade_in_animation_.Start();
  }
  if (is_default_favicon || ShouldThemifyFaviconForUrl(url)) {
    themed_favicon_ = ThemeImage(icon);
  } else {
    themed_favicon_ = gfx::ImageSkia();
  }
  SchedulePaint();
}

void TabIcon::SetNetworkState(TabNetworkState network_state,
                              float load_progress) {
  if (network_state_ != network_state) {
    TabNetworkState old_state = network_state_;
    network_state_ = network_state;

    if (network_state_ == TabNetworkState::kLoading) {
      // When transitioning to loading, reset the progress indicatator + timer.
      loading_progress_ = 0.0;
      loading_progress_timer_.SetCurrentValue(0);
    }

    if (old_state == TabNetworkState::kLoading) {
      loading_progress_ = 1.0;
      // This fades in the placeholder favicon if no favicon has loaded so far.
      if (!favicon_fade_in_animation_.is_animating() &&
          favicon_fade_in_animation_.GetCurrentValue() == 0.0) {
        favicon_fade_in_animation_.Start();
      }
    }

    if (network_state_ == TabNetworkState::kWaiting) {
      // Reset favicon and tab-loading animations
      favicon_fade_in_animation_.Stop();
      favicon_fade_in_animation_.SetCurrentValue(0.0);
      loading_progress_timer_.Stop();
      loading_progress_timer_.SetCurrentValue(0.0);
    }
    SchedulePaint();
  }

  // The loading progress looks really weird if it ever jumps backwards, so make
  // sure it only increases.
  if (network_state_ == TabNetworkState::kLoading &&
      loading_progress_ < load_progress) {
    // Clamp the loading progress timer to the currently visible value. Note
    // that the animation plays a fade-out effect after loading_progress_ is
    // full, so filling the bar only uses a fraction of the timer.
    loading_progress_timer_.SetCurrentValue(
        std::min(loading_progress_timer_.GetCurrentValue(),
                 loading_progress_ / kProgressIndicatorAnimationDuration));
    loading_progress_ = load_progress;
    loading_progress_timer_.Start();
  }
}

void TabIcon::SetIsCrashed(bool is_crashed) {
  if (is_crashed == is_crashed_)
    return;
  is_crashed_ = is_crashed;

  if (!is_crashed_) {
    // Transitioned from crashed to non-crashed.
    if (crash_animation_)
      crash_animation_->Stop();
    should_display_crashed_favicon_ = false;
    hiding_fraction_ = 0.0;
  } else {
    // Transitioned from non-crashed to crashed.
    if (!crash_animation_)
      crash_animation_ = std::make_unique<CrashAnimation>(this);
    if (!crash_animation_->is_animating())
      crash_animation_->Start();
  }
  SchedulePaint();
}

void TabIcon::RefreshLayer() {
  // Since the loading animation can run for a long time, paint animation to a
  // separate layer when possible to reduce repaint overhead.
  bool should_paint_to_layer = can_paint_to_layer_ && ShowingLoadingAnimation();
  if (should_paint_to_layer == !!layer())
    return;

  // Change layer mode.
  if (should_paint_to_layer) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  } else {
    DestroyLayer();
  }
}

gfx::ImageSkia TabIcon::ThemeImage(const gfx::ImageSkia& source) {
  return gfx::ImageSkiaOperations::CreateHSLShiftedImage(
      source, GetThemeProvider()->GetTint(ThemeProperties::TINT_BUTTONS));
}
