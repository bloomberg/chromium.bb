// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_icon.h"

#include "base/time/default_tick_clock.h"
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

constexpr int kLoadingProgressTimeMs = 400;
constexpr int kLoadingProgressFadeOutMs = 200;
constexpr int kFaviconFadeInMs = 200;

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

TabIcon::TabIcon() : clock_(base::DefaultTickClock::GetInstance()) {
  set_can_process_events_within_subtree(false);

  // The minimum size to avoid clipping the attention indicator.
  SetPreferredSize(gfx::Size(gfx::kFaviconSize + kAttentionIndicatorRadius,
                             gfx::kFaviconSize + kAttentionIndicatorRadius));

  UpdateLoadingAnimationState();
  // Initial state (before any data) should not be animating.
  DCHECK(!ShowingLoadingAnimation());
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
    old_animation_loading_start_time_ = base::TimeTicks();
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

  if (network_state_ != TabNetworkState::kNone &&
      network_state_ != TabNetworkState::kError)
    return true;

  const base::TimeTicks now = clock_->NowTicks();
  if (loading_progress_timer_ &&
      *loading_progress_timer_ +
              base::TimeDelta::FromMilliseconds(kLoadingProgressTimeMs +
                                                kLoadingProgressFadeOutMs) >=
          now) {
    return true;
  }

  if (favicon_fade_in_animation_ &&
      *favicon_fade_in_animation_ +
              base::TimeDelta::FromMilliseconds(kFaviconFadeInMs) >=
          now) {
    return true;
  }

  // If the last frame painted still displays the loading indicator or favicon
  // in less than full opacity we need to paint the last frame.
  if (animation_state_.favicon_alpha < SK_AlphaOPAQUE ||
      animation_state_.loading_progress_alpha > 0) {
    return true;
  }

  return false;
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

  if (LoadingAnimationNeedsRepaint())
    SchedulePaint();

  RefreshLayer();
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

  UpdateLoadingAnimationState();

  if (ShowingAttentionIndicator() && !should_display_crashed_favicon_) {
    PaintAttentionIndicatorAndIcon(canvas, GetIconToPaint(), icon_bounds);
  } else if (!MaybePaintFavicon(canvas, GetIconToPaint(), icon_bounds)) {
    PaintFaviconPlaceholder(canvas, icon_bounds);
  }

  if (ShowingLoadingAnimation())
    PaintLoadingAnimation(canvas, icon_bounds);
}

void TabIcon::UpdateLoadingAnimationState() {
  animation_state_ = GetLoadingAnimationState();
}

TabIcon::LoadingAnimationState TabIcon::GetLoadingAnimationState() const {
  const base::TimeTicks now = clock_->NowTicks();
  LoadingAnimationState state;

  state.elapsed_time = waiting_state_.elapsed_time;

  if (!loading_progress_timer_) {
    state.loading_progress = 0;
    state.loading_progress_alpha = 0;
  } else {
    base::TimeDelta loading_progress_time = now - *loading_progress_timer_;

    state.loading_progress = std::min(
        loading_progress_time.InMillisecondsF() / kLoadingProgressTimeMs,
        target_loading_progress_);
    state.loading_progress_alpha = SK_AlphaOPAQUE;
    if (network_state_ != TabNetworkState::kLoading &&
        loading_progress_time.InMilliseconds() > kLoadingProgressTimeMs) {
      // When we're no longer loading, translate loading progress time above
      // kLoadingProgressTimeMs to the fade-out effect.
      state.loading_progress_alpha =
          SK_AlphaOPAQUE *
          std::max(1.0 - (loading_progress_time.InMillisecondsF() -
                          kLoadingProgressTimeMs) /
                             kLoadingProgressFadeOutMs,
                   0.0);
    }
  }

  // In the waiting/loading state we initially show no favicon.
  state.favicon_alpha = (network_state_ == TabNetworkState::kWaiting ||
                         network_state_ == TabNetworkState::kLoading)
                            ? 0
                            : SK_AlphaOPAQUE;
  if (favicon_fade_in_animation_) {
    base::TimeDelta favicon_fade_in_time = now - *favicon_fade_in_animation_;
    state.favicon_alpha =
        SK_AlphaOPAQUE *
        std::min(favicon_fade_in_time.InMillisecondsF() / kFaviconFadeInMs,
                 1.0);
  }

  return state;
}

bool TabIcon::LoadingAnimationNeedsRepaint() const {
  if (!UseNewLoadingAnimation())
    return ShowingLoadingAnimation();

  // Throbber always needs a repaint.
  if (network_state_ == TabNetworkState::kWaiting)
    return true;

  LoadingAnimationState new_state = GetLoadingAnimationState();

  // Compare without |elapsed_time| as it's only used in the waiting state.
  auto tie = [](const LoadingAnimationState& state) {
    return std::tie(state.loading_progress, state.loading_progress_alpha,
                    state.favicon_alpha);
  };

  return tie(new_state) != tie(animation_state_);
}

void TabIcon::OnThemeChanged() {
  crashed_icon_ = gfx::ImageSkia();  // Force recomputation if crashed.
  if (!themed_favicon_.isNull())
    themed_favicon_ = ThemeImage(favicon_);
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
  bounds.set_width(bounds.height() + animation_state_.loading_progress *
                                         (bounds.width() - bounds.height()));

  cc::PaintFlags flags;
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  // Disable anti-aliasing to effectively "pixel align" the rectangle.
  flags.setAntiAlias(false);
  flags.setAlpha(animation_state_.loading_progress_alpha);

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
                                   animation_state_.elapsed_time);
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
      const base::TimeTicks current_time = clock_->NowTicks();
      if (old_animation_loading_start_time_.is_null())
        old_animation_loading_start_time_ = current_time;

      waiting_state_.color =
          tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_WAITING);
      gfx::PaintThrobberSpinningAfterWaiting(
          canvas, bounds,
          tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_SPINNING),
          current_time - old_animation_loading_start_time_, &waiting_state_);
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
  if (animation_state_.favicon_alpha == 0)
    return false;

  if (icon.isNull())
    return false;

  cc::PaintFlags flags;
  flags.setAlpha(animation_state_.favicon_alpha);

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

  if (!is_default_favicon && !favicon_fade_in_animation_)
    favicon_fade_in_animation_ = clock_->NowTicks();

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
      target_loading_progress_ = 0.0;
      loading_progress_timer_ = clock_->NowTicks();
    }

    if (old_state == TabNetworkState::kLoading) {
      target_loading_progress_ = 1.0;
      // Start fading in placeholder favicon if no favicon has loaded so far.
      const base::TimeTicks now = clock_->NowTicks();
      if (!favicon_fade_in_animation_)
        favicon_fade_in_animation_ = now;

      // Rewind the progress timer back to 100% if necessary. This prevents
      // parts of the fade-out animation to be skipped.
      RewindLoadingProgressTimerIfNecessary(1.0f);
    }

    if (network_state_ == TabNetworkState::kWaiting) {
      // Reset favicon and tab-loading animations
      favicon_fade_in_animation_.reset();
      loading_progress_timer_.reset();
    }
    SchedulePaint();
  }

  // The loading progress looks really weird if it ever jumps backwards, so make
  // sure it only increases.
  if (network_state_ == TabNetworkState::kLoading &&
      target_loading_progress_ < load_progress) {
    DCHECK(loading_progress_timer_);

    RewindLoadingProgressTimerIfNecessary(target_loading_progress_);

    target_loading_progress_ = load_progress;
  }
}

void TabIcon::RewindLoadingProgressTimerIfNecessary(double progress) {
  base::TimeTicks clamped_start =
      clock_->NowTicks() -
      base::TimeDelta::FromMilliseconds(progress * kLoadingProgressTimeMs);
  if (clamped_start > loading_progress_timer_)
    loading_progress_timer_ = clamped_start;
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
