// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_ICON_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/view.h"

class GURL;
struct TabRendererData;

// View that displays the favicon, sad tab, throbber, and attention indicator
// in a tab.
//
// The icon will be drawn in the upper left (upper right for RTL). Normally you
// would lay this out so the top is where you want the icon to be positioned,
// the width is TabIcon::GetIdealWidth(), and the height goes down to the
// bottom of the enclosing view (this is so the crashed tab can animate out of
// the bottom).
class TabIcon : public views::View {
 public:
  // Attention indicator types (use as a bitmask). There is only one visual
  // representation, but the state of each of these is tracked separately and
  // the indicator is shown as long as one is enabled.
  enum class AttentionType {
    kBlockedWebContents = 1 << 0,       // The WebContents is marked as blocked.
    kTabWantsAttentionStatus = 1 << 1,  // Tab::SetTabNeedsAttention() called.
  };

  TabIcon();
  ~TabIcon() override;

  // Sets the tab data (network state, favicon, load progress, etc.) that are
  // used to render the tab icon.
  void SetData(const TabRendererData& data);

  // Enables or disables the given attention type. The attention indicator
  // will be shown as long as any of the types are enabled.
  void SetAttention(AttentionType type, bool enabled);

  bool ShowingLoadingAnimation() const;
  bool ShowingAttentionIndicator() const;

  // Sets whether this object can paint to a layer. When the loading animation
  // is running, painting to a layer saves painting overhead. But if the tab is
  // being painted to some other context than the window, the layered painting
  // won't work.
  void SetCanPaintToLayer(bool can_paint_to_layer);

  // The loading animation only steps when this function is called. The
  // |elapsed_time| parameter is expected to be the same among all tabs in a tab
  // strip in order to keep the throbbers in sync.
  void StepLoadingAnimation(const base::TimeDelta& elapsed_time);

  void SetBackgroundColor(SkColor color);

 private:
  class CrashAnimation;
  friend CrashAnimation;
  friend class TabTest;

  // State used to draw the tab-loading animation. Also used to store the
  // last-painted state to know to redraw the final frame as the animation
  // finishes.
  struct LoadingAnimationState {
    base::TimeDelta elapsed_time;
    double loading_progress;
    SkAlpha loading_progress_alpha;
    SkAlpha favicon_alpha;
  };

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Paints the attention indicator and |favicon_| at the given location.
  void PaintAttentionIndicatorAndIcon(gfx::Canvas* canvas,
                                      const gfx::ImageSkia& icon,
                                      const gfx::Rect& bounds);

  // Draws a progress bar at the bottom of the tab icon to indicate loading
  // progress.
  void PaintLoadingProgressIndicator(gfx::Canvas* canvas,
                                     gfx::RectF bounds,
                                     SkColor color);

  // Paint either the indeterimate throbber or progress indicator according to
  // current tab state.
  void PaintLoadingAnimation(gfx::Canvas* canvas, const gfx::Rect& bounds);

  void UpdateLoadingAnimationState();
  LoadingAnimationState GetLoadingAnimationState() const;

  void RewindLoadingProgressTimerIfNecessary(double progress);

  // Returns false if painting the loading animation would paint the same thing
  // that's already painted.
  bool LoadingAnimationNeedsRepaint() const;

  // Gets either the crashed icon or favicon to be rendered for the tab.
  const gfx::ImageSkia& GetIconToPaint();

  // Paints a placeholder image for the favicon if one should be painted.
  void PaintFaviconPlaceholder(gfx::Canvas* canvas, const gfx::Rect& bounds);
  // Paint the favicon if it's available. Returns true if the icon was painted.
  bool MaybePaintFavicon(gfx::Canvas* canvas,
                         const gfx::ImageSkia& icon,
                         const gfx::Rect& bounds);

  // Sets the icon. Depending on the URL the icon may be automatically themed.
  void SetIcon(const GURL& url, const gfx::ImageSkia& favicon);

  // For certain types of tabs the loading animation is not desired so the
  // caller can set inhibit_loading_animation to true. When false, the loading
  // animation state will be derived from the network state.
  void SetNetworkState(TabNetworkState network_state, float load_progress);

  // Sets whether the tab should paint as crashed or not.
  void SetIsCrashed(bool is_crashed);

  // Creates or destroys the layer according to the current animation state and
  // whether a layer can be used.
  void RefreshLayer();

  void UpdateThemedFavicon();

  gfx::ImageSkia ThemeImage(const gfx::ImageSkia& source);

  const base::TickClock* clock_;

  gfx::ImageSkia favicon_;
  TabNetworkState network_state_ = TabNetworkState::kNone;
  bool is_crashed_ = false;
  int attention_types_ = 0;  // Bitmask of AttentionType.

  // Value from last call to SetNetworkState. When true, the network loading
  // animation will not be shown.
  bool inhibit_loading_animation_ = false;

  // The point in time when the tab icon was first painted in the loading state.
  // TODO(pbos): Remove after |kNewTabLoadingAnimation| launches.
  base::TimeTicks old_animation_loading_start_time_;

  // Paint state for the loading animation after the most recent waiting paint.
  // TODO(pbos): After |kNewTabLoadingAnimation| launches, remove the need for
  // |waiting_state_|, the new animation only uses the |elapsed_time| member.
  gfx::ThrobberWaitingState waiting_state_;

  // When the favicon_ has theming applied to it, the themed version will be
  // cached here. If this isNull(), then there is no theming and favicon_
  // should be used.
  gfx::ImageSkia themed_favicon_;

  // May be different than is_crashed when the crashed icon is animating in.
  bool should_display_crashed_favicon_ = false;

  // Drawn when should_display_crashed_favicon_ is set. Created lazily.
  gfx::ImageSkia crashed_icon_;

  // The fraction the icon is hidden by for the crashed tab animation.
  // When this is 0 it will be drawn at the normal location, and when this is 1
  // it will be drawn off the bottom.
  double hiding_fraction_ = 0.0;

  // Loading progress used for drawing the progress indicator.
  double target_loading_progress_ = 1.0;

  LoadingAnimationState animation_state_;

  base::Optional<base::TimeTicks> loading_progress_timer_;

  // Fade-in animation for the favicon. Starts when a favicon loads or the tab
  // is no longer loading. The latter case will fade into a placeholder icon.
  base::Optional<base::TimeTicks> favicon_fade_in_animation_;

  // Crash animation (in place of favicon). Lazily created since most of the
  // time it will be unneeded.
  std::unique_ptr<CrashAnimation> crash_animation_;

  bool can_paint_to_layer_ = false;

  SkColor bg_color_ = SK_ColorBLACK;

  DISALLOW_COPY_AND_ASSIGN(TabIcon);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_ICON_H_
