// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include <stddef.h>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/debug/alias.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/touch_uma/touch_uma.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

using base::UserMetricsAction;
using MD = ui::MaterialDesignController;

namespace {

constexpr int kExtraLeftPaddingToBalanceCloseButtonPadding = 2;
constexpr int kRefreshExtraLeftPaddingToBalanceCloseButtonPadding = 4;

constexpr int kRefreshAlertIndicatorCloseButtonPadding = 6;
constexpr int kTouchableRefreshAlertIndicatorCloseButtonPadding = 8;

// When a non-pinned tab becomes a pinned tab the width of the tab animates. If
// the width of a pinned tab is at least kPinnedTabExtraWidthToRenderAsNormal
// larger than the desired pinned tab width then the tab is rendered as a normal
// tab. This is done to avoid having the title immediately disappear when
// transitioning a tab from normal to pinned tab.
constexpr int kPinnedTabExtraWidthToRenderAsNormal = 30;

// Opacity of the active tab background painted over inactive selected tabs.
constexpr float kSelectedTabOpacity = 0.75f;

// Inactive selected tabs have their throb value scaled by this.
constexpr float kSelectedTabThrobScale = 0.95f - kSelectedTabOpacity;

// Height of the separator painted on the left edge of the tab for the material
// refresh mode.
constexpr int kTabSeparatorHeight = 20;
constexpr int kTabSeparatorTouchHeight = 24;

// Helper functions ------------------------------------------------------------

// Returns the coordinate for an object of size |item_size| centered in a region
// of size |size|, biasing towards placing any extra space ahead of the object.
int Center(int size, int item_size) {
  int extra_space = size - item_size;
  // Integer division below truncates, thus effectively "rounding toward zero";
  // to always place extra space ahead of the object, we want to round towards
  // positive infinity, which means we need to bias the division only when the
  // size difference is positive.  (Adding one unconditionally will stack with
  // the truncation if |extra_space| is negative, resulting in off-by-one
  // errors.)
  if (extra_space > 0)
    ++extra_space;
  return extra_space / 2;
}

// For non-material-refresh mode, returns the width of the tab endcap in DIP.
// More precisely, this is the width of the curve making up either the outer or
// inner edge of the stroke.
//
// These two curves are horizontally offset by 1 px (regardless of scale); the
// total width of the endcap from tab outer edge to the inside end of the stroke
// inner edge is (GetTabEndcapWidthForLayout() * scale) + 1.
int GetTabEndcapWidthForLayout() {
  const int mode = MD::GetMode();
  DCHECK_LE(mode, 2);

  constexpr int kEndcapWidth[] = {16, 18, 24};
  return kEndcapWidth[mode];
}

// For painting the endcaps, the top corners are actually shifted outwards 0.5
// DIP from the grid.
float GetTabEndcapWidthForPainting() {
  return GetTabEndcapWidthForLayout() - 0.5f;
}

void DrawHighlight(gfx::Canvas* canvas,
                   const SkPoint& p,
                   SkScalar radius,
                   SkColor color) {
  const SkColor colors[2] = { color, SkColorSetA(color, 0) };
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeRadialGradient(
      p, radius, colors, nullptr, 2, SkShader::kClamp_TileMode));
  canvas->sk_canvas()->drawRect(
      SkRect::MakeXYWH(p.x() - radius, p.y() - radius, radius * 2, radius * 2),
      flags);
}

// Scales |bounds| by scale and aligns so that adjacent tabs meet up exactly
// during painting.
const gfx::RectF ScaleAndAlignBounds(const gfx::Rect& bounds,
                                     float scale,
                                     float stroke_thickness) {
  // Convert to layout bounds.  We must inset the width such that the right edge
  // of one tab's layout bounds is the same as the left edge of the next tab's;
  // this way the two tabs' separators will be drawn at the same coordinate.
  gfx::RectF aligned_bounds(bounds);
  const int corner_radius = Tab::GetCornerRadius();
  // Note: This intentionally doesn't subtract TABSTRIP_TOOLBAR_OVERLAP from the
  // bottom inset, because we want to pixel-align the bottom of the stroke, not
  // the bottom of the overlap.
  gfx::InsetsF layout_insets(stroke_thickness, corner_radius, stroke_thickness,
                             corner_radius + Tab::kSeparatorThickness);
  aligned_bounds.Inset(layout_insets);

  // Scale layout bounds from DIP to px.
  aligned_bounds.Scale(scale);

  // Snap layout bounds to nearest pixels so we get clean lines.
  const float x = std::round(aligned_bounds.x());
  const float y = std::round(aligned_bounds.y());
  // It's important to round the right edge and not the width, since rounding
  // both x and width would mean the right edge would accumulate error.
  const float right = std::round(aligned_bounds.right());
  const float bottom = std::round(aligned_bounds.bottom());
  aligned_bounds = gfx::RectF(x, y, right - x, bottom - y);

  // Convert back to full bounds.  It's OK that the outer corners of the curves
  // around the separator may not be snapped to the pixel grid as a result.
  aligned_bounds.Inset(-layout_insets.Scale(scale));
  return aligned_bounds;
}

// Offsets each path inward by |insets|, then intersects them together.
gfx::Path OffsetAndIntersectPaths(gfx::Path& left_path,
                                  gfx::Path& right_path,
                                  const gfx::InsetsF& insets) {
  // This code is not prepared to deal with vertical adjustments.
  DCHECK_EQ(0, insets.top());
  DCHECK_EQ(0, insets.bottom());

  gfx::Path complete_path;
  left_path.offset(insets.left(), 0);
  right_path.offset(-insets.right(), 0);
  Op(left_path, right_path, SkPathOp::kIntersect_SkPathOp, &complete_path);
  return complete_path;
}

// Given a tab of width |width|, returns the radius to use for the corners.
float GetTopCornerRadiusForWidth(int width) {
  // Get the width of the top of the tab by subtracting the width of the outer
  // corners.
  const int ideal_radius = Tab::GetCornerRadius();
  const int top_width = width - ideal_radius * 2;

  // To maintain a round-rect appearance, ensure at least one third of the top
  // of the tab is flat.
  const float radius = top_width / 3.f;
  return base::ClampToRange<float>(radius, 0, ideal_radius);
}

// The refresh-specific implementation of GetInteriorPath() (see below).
gfx::Path GetRefreshInteriorPath(float scale,
                                 float stroke_thickness,
                                 float bottom_offset,
                                 const gfx::Rect& bounds,
                                 const gfx::InsetsF& insets) {
  const gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(bounds, scale, stroke_thickness);

  stroke_thickness *= scale;
  bottom_offset *= scale;

  const float radius = GetTopCornerRadiusForWidth(bounds.width()) * scale;
  const float bottom_radius = radius - bottom_offset;
  const float top_radius = std::max(radius - stroke_thickness, 0.f);

  // Compute |extension| as the width outside the separators.  This is a fixed
  // value equal to the normal corner radius.
  const float extension = Tab::GetCornerRadius() * scale;

  // When the radius shrinks, it leaves a gap between the bottom corners and the
  // edge of the tab.
  const float corner_gap = extension - bottom_radius;

  const float left = aligned_bounds.x() + stroke_thickness;
  const float top = aligned_bounds.y() + stroke_thickness;
  const float right = aligned_bounds.right() - stroke_thickness;

  // Overlap the toolbar below us so that gaps don't occur when rendering at
  // non-integral display scale factors.
  const float extended_bottom = aligned_bounds.bottom();
  const float bottom_extension =
      GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) * scale;
  const float bottom = extended_bottom - bottom_extension - bottom_offset;

  // Construct the interior path by intersecting paths representing the left
  // and right halves of the tab.  Compared to computing the full path at once,
  // this makes it easier to avoid overdraw in the top center near minimum
  // width, and to implement cases where !insets.IsEmpty().

  // Bottom right.
  gfx::Path right_path;
  right_path.moveTo(right - corner_gap, extended_bottom);
  right_path.lineTo(right - corner_gap, bottom);
  right_path.rArcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
                    SkPath::kCW_Direction, -bottom_radius, -bottom_radius);

  // Right vertical.
  right_path.lineTo(right - extension, top + top_radius);

  // Top right.
  right_path.rArcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                    SkPath::kCCW_Direction, -top_radius, -top_radius);

  // Top/bottom edges of right side.
  right_path.lineTo(left, top);
  right_path.lineTo(left, extended_bottom);
  right_path.close();

  // Top left.
  gfx::Path left_path;
  left_path.moveTo(left + extension + top_radius, top);
  left_path.rArcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                   SkPath::kCCW_Direction, -top_radius, top_radius);

  // Left vertical.
  left_path.lineTo(left + extension, bottom - bottom_radius);

  // Bottom left.
  left_path.rArcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
                   SkPath::kCW_Direction, -bottom_radius, bottom_radius);
  left_path.lineTo(left + corner_gap, extended_bottom);

  // Bottom/top edges of left side.
  left_path.lineTo(right, extended_bottom);
  left_path.lineTo(right, top);
  left_path.close();

  // Convert paths to be relative to the tab origin.
  gfx::PointF origin(bounds.origin());
  origin.Scale(scale);
  right_path.offset(-origin.x(), -origin.y());
  left_path.offset(-origin.x(), -origin.y());

  return OffsetAndIntersectPaths(left_path, right_path, insets.Scale(scale));
}

// Returns a path corresponding to the tab's content region inside the outer
// stroke. The sides of the path will be inset by |insets|; this is useful when
// trying to clip favicons to match the overall tab shape but be inset from the
// edge.
gfx::Path GetInteriorPath(float scale,
                          float stroke_thickness,
                          float bottom_offset,
                          const gfx::Rect& bounds,
                          const gfx::InsetsF& insets = gfx::InsetsF()) {
  if (MD::IsRefreshUi())
    return GetRefreshInteriorPath(scale, stroke_thickness, bottom_offset,
                                  bounds, insets);

  const float right = bounds.width() * scale;
  // The bottom of the tab needs to be pixel-aligned or else when we call
  // ClipPath with anti-aliasing enabled it can cause artifacts.
  const float bottom = std::ceil(bounds.height() * scale);
  const float endcap_width = GetTabEndcapWidthForPainting();

  // Construct the interior path by intersecting paths representing the left
  // and right halves of the tab.  Compared to computing the full path at once,
  // this makes it easier to avoid overdraw in the top center near minimum
  // width, and to implement cases where !insets.IsEmpty().

  gfx::Path right_path;
  right_path.moveTo(right - 1, bottom);
  right_path.rCubicTo(-0.75 * scale, 0, -1.625 * scale, -0.5 * scale,
                      -2 * scale, -1.5 * scale);
  right_path.lineTo(right - 1 - (endcap_width - 2) * scale, 2.5 * scale);
  right_path.rCubicTo(-0.375 * scale, -1 * scale, -1.25 * scale, -1.5 * scale,
                      -2 * scale, -1.5 * scale);
  right_path.lineTo(0, scale);
  right_path.lineTo(0, bottom);
  right_path.close();

  gfx::Path left_path;
  left_path.moveTo(1 + endcap_width * scale, scale);
  left_path.rCubicTo(-0.75 * scale, 0, -1.625 * scale, 0.5 * scale, -2 * scale,
                     1.5 * scale);
  left_path.lineTo(1 + 2 * scale, bottom - 1.5 * scale);
  left_path.rCubicTo(-0.375 * scale, scale, -1.25 * scale, 1.5 * scale,
                     -2 * scale, 1.5 * scale);
  left_path.lineTo(right, bottom);
  left_path.lineTo(right, scale);
  left_path.close();

  return OffsetAndIntersectPaths(left_path, right_path, insets.Scale(scale));
}

// The refresh-specific implementation of GetBorderPath() (see below).
gfx::Path GetRefreshBorderPath(const gfx::Rect& bounds,
                               bool extend_to_top,
                               float scale,
                               float stroke_thickness,
                               float bottom_offset) {
  const gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(bounds, scale, stroke_thickness);

  stroke_thickness *= scale;
  bottom_offset *= scale;

  const float top_radius = GetTopCornerRadiusForWidth(bounds.width()) * scale;
  const float bottom_radius =
      std::max(top_radius - stroke_thickness, 0.f) - bottom_offset;

  // See comments in GetRefreshInteriorPath().
  const float extension = Tab::GetCornerRadius() * scale;
  const float corner_gap = extension - bottom_radius;

  const float left = aligned_bounds.x();
  const float top = aligned_bounds.y();
  const float right = aligned_bounds.right();

  // Overlap the toolbar below us so that gaps don't occur when rendering at
  // non-integral display scale factors.
  const float extended_bottom = aligned_bounds.bottom();
  const float bottom_extension =
      GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) * scale;
  const float bottom = extended_bottom - bottom_extension - bottom_offset;

  // Bottom left.
  gfx::Path path;
  path.moveTo(left, extended_bottom);
  path.lineTo(left, bottom - stroke_thickness);
  path.rLineTo(corner_gap, 0);
  path.rArcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
              SkPath::kCCW_Direction, bottom_radius, -bottom_radius);

  if (extend_to_top) {
    // Left vertical.
    path.lineTo(left + extension, top);

    // Top edge.
    path.lineTo(right - extension, top);
  } else {
    // Left vertical.
    path.lineTo(left + extension, top + top_radius);

    // Top left.
    path.rArcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                SkPath::kCW_Direction, top_radius, -top_radius);

    // Top edge.
    path.lineTo(right - extension - top_radius, top);

    // Top right.
    path.rArcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                SkPath::kCW_Direction, top_radius, top_radius);
  }

  // Right vertical.
  path.lineTo(right - extension, bottom - stroke_thickness - bottom_radius);

  // Bottom right.
  path.rArcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
              SkPath::kCCW_Direction, bottom_radius, bottom_radius);
  path.rLineTo(corner_gap, 0);
  path.rLineTo(0, stroke_thickness);
  path.rLineTo(0, bottom_extension);

  // Bottom edge.
  path.close();

  // Convert path to be relative to the tab origin.
  gfx::PointF origin(bounds.origin());
  origin.Scale(scale);
  path.offset(-origin.x(), -origin.y());

  return path;
}

// Returns a path corresponding to the tab's outer border for a given tab
// |scale| and |bounds|.  If |unscale_at_end| is true, this path will be
// normalized to a 1x scale by scaling by 1/scale before returning.  If
// |extend_to_top| is true, the path is extended vertically to the top of the
// tab bounds.  The caller uses this for Fitts' Law purposes in
// maximized/fullscreen mode.
gfx::Path GetBorderPath(float scale,
                        float stroke_thickness,
                        float bottom_offset,
                        bool unscale_at_end,
                        bool extend_to_top,
                        const gfx::Rect& bounds) {
  gfx::Path path;
  if (MD::IsRefreshUi()) {
    path = GetRefreshBorderPath(bounds, extend_to_top, scale, stroke_thickness,
                                bottom_offset);
  } else {
    const float top = scale - stroke_thickness;
    const float right = bounds.width() * scale;
    const float bottom = bounds.height() * scale;
    const float endcap_width = GetTabEndcapWidthForPainting();

    path.moveTo(0, bottom);
    path.rLineTo(0, -stroke_thickness);
    path.rCubicTo(0.75 * scale, 0, 1.625 * scale, -0.5 * scale, 2 * scale,
                  -1.5 * scale);
    path.lineTo((endcap_width - 2) * scale, top + 1.5 * scale);
    if (extend_to_top) {
      // Create the vertical extension by extending the side diagonals until
      // they reach the top of the bounds.
      const float dy = 2.5 * scale - stroke_thickness;
      const float dx = Tab::GetInverseDiagonalSlope() * dy;
      path.rLineTo(dx, -dy);
      path.lineTo(right - (endcap_width - 2) * scale - dx, 0);
      path.rLineTo(dx, dy);
    } else {
      path.rCubicTo(0.375 * scale, -scale, 1.25 * scale, -1.5 * scale,
                    2 * scale, -1.5 * scale);
      path.lineTo(right - endcap_width * scale, top);
      path.rCubicTo(0.75 * scale, 0, 1.625 * scale, 0.5 * scale, 2 * scale,
                    1.5 * scale);
    }
    path.lineTo(right - 2 * scale, bottom - stroke_thickness - 1.5 * scale);
    path.rCubicTo(0.375 * scale, scale, 1.25 * scale, 1.5 * scale, 2 * scale,
                  1.5 * scale);
    path.rLineTo(0, stroke_thickness);
    path.close();
  }

  if (unscale_at_end && (scale != 1))
    path.transform(SkMatrix::MakeScale(1.f / scale));

  return path;
}

}  // namespace

// Tab -------------------------------------------------------------------------

// static
const char Tab::kViewClassName[] = "Tab";

Tab::Tab(TabController* controller, gfx::AnimationContainer* container)
    : controller_(controller),
      pulse_animation_(this),
      animation_container_(container),
      title_(new views::Label()),
      title_animation_(this),
      hover_controller_(this) {
  DCHECK(controller);

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  set_notify_enter_exit_on_child(true);

  set_id(VIEW_ID_TAB);

  // This will cause calls to GetContentsBounds to return only the rectangle
  // inside the tab shape, rather than to its extents.
  SetBorder(views::CreateEmptyBorder(GetContentsInsets()));

  title_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_->SetElideBehavior(gfx::FADE_TAIL);
  title_->SetHandlesTooltips(false);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetText(CoreTabHelper::GetDefaultTitle());
  AddChildView(title_);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  icon_ = new TabIcon;
  AddChildView(icon_);

  alert_indicator_button_ = new AlertIndicatorButton(this);
  AddChildView(alert_indicator_button_);

  // Unretained is safe here because this class outlives its close button, and
  // the controller outlives this Tab.
  close_button_ = new TabCloseButton(
      this, base::BindRepeating(&TabController::OnMouseEventInTab,
                                base::Unretained(controller_)));
  AddChildView(close_button_);

  set_context_menu_controller(this);

  constexpr int kPulseDurationMs = 200;
  pulse_animation_.SetSlideDuration(kPulseDurationMs);
  pulse_animation_.SetContainer(animation_container_.get());

  title_animation_.SetDuration(base::TimeDelta::FromMilliseconds(100));
  title_animation_.SetContainer(animation_container_.get());

  hover_controller_.SetAnimationContainer(animation_container_.get());
}

Tab::~Tab() {
}

void Tab::AnimationEnded(const gfx::Animation* animation) {
  if (animation == &title_animation_)
    title_->SetBoundsRect(target_title_bounds_);
  else
    SchedulePaint();
}

void Tab::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &title_animation_) {
    title_->SetBoundsRect(gfx::Tween::RectValueBetween(
        gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                   animation->GetCurrentValue()),
        start_title_bounds_, target_title_bounds_));
    return;
  }

  // Ignore if the pulse animation is being performed on active tab because
  // it repaints the same image. See PaintTab().
  if (animation == &pulse_animation_ && IsActive())
    return;

  SchedulePaint();
}

void Tab::AnimationCanceled(const gfx::Animation* animation) {
  SchedulePaint();
}

void Tab::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (!alert_indicator_button_ || !alert_indicator_button_->visible())
    base::RecordAction(UserMetricsAction("CloseTab_NoAlertIndicator"));
  else if (alert_indicator_button_->enabled())
    base::RecordAction(UserMetricsAction("CloseTab_MuteToggleAvailable"));
  else if (data_.alert_state == TabAlertState::AUDIO_PLAYING)
    base::RecordAction(UserMetricsAction("CloseTab_AudioIndicator"));
  else
    base::RecordAction(UserMetricsAction("CloseTab_RecordingIndicator"));

  const CloseTabSource source =
      (event.type() == ui::ET_MOUSE_RELEASED &&
       !(event.flags() & ui::EF_FROM_TOUCH)) ? CLOSE_TAB_FROM_MOUSE
                                             : CLOSE_TAB_FROM_TOUCH;
  DCHECK_EQ(close_button_, sender);
  controller_->CloseTab(this, source);
  if (event.type() == ui::ET_GESTURE_TAP)
    TouchUMA::RecordGestureAction(TouchUMA::kGestureTabCloseTap);
}

void Tab::ShowContextMenuForView(views::View* source,
                                 const gfx::Point& point,
                                 ui::MenuSourceType source_type) {
  if (!closing_)
    controller_->ShowContextMenuForTab(this, point, source_type);
}

bool Tab::GetHitTestMask(gfx::Path* mask) const {
  // When the window is maximized we don't want to shave off the edges or top
  // shadow of the tab, such that the user can click anywhere along the top
  // edge of the screen to select a tab. Ditto for immersive fullscreen.
  const views::Widget* widget = GetWidget();
  *mask = GetBorderPath(
      GetWidget()->GetCompositor()->device_scale_factor(), GetStrokeThickness(),
      IsActive() ? 0 : controller_->GetStrokeThickness(), true,
      widget && (widget->IsMaximized() || widget->IsFullscreen()), bounds());
  return true;
}

void Tab::Layout() {
  const gfx::Rect contents_rect = GetContentsBounds();

  const bool was_showing_icon = showing_icon_;
  UpdateIconVisibility();

  int extra_left_padding = 0;
  if (extra_padding_before_content_) {
    extra_left_padding =
        MD::IsRefreshUi() ? kRefreshExtraLeftPaddingToBalanceCloseButtonPadding
                          : kExtraLeftPaddingToBalanceCloseButtonPadding;
  }

  const int start = contents_rect.x() + extra_left_padding;

  // The bounds for the favicon will include extra width for the attention
  // indicator, but visually it will be smaller at kFaviconSize wide.
  gfx::Rect favicon_bounds(start, contents_rect.y(), 0, 0);
  if (showing_icon_) {
    // Height should go to the bottom of the tab for the crashed tab animation
    // to pop out of the bottom.
    favicon_bounds.set_y(contents_rect.y() +
                         Center(contents_rect.height(), gfx::kFaviconSize));
    favicon_bounds.set_size(
        gfx::Size(icon_->GetPreferredSize().width(),
                  contents_rect.height() - favicon_bounds.y()));
    if (center_favicon_) {
      // When centering the favicon, the favicon is allowed to escape the normal
      // contents rect.
      favicon_bounds.set_x(Center(width(), gfx::kFaviconSize));
    } else {
      MaybeAdjustLeftForPinnedTab(&favicon_bounds, gfx::kFaviconSize);
    }
  }
  icon_->SetBoundsRect(favicon_bounds);
  icon_->SetVisible(showing_icon_);

  const int after_title_padding = GetLayoutConstant(TAB_AFTER_TITLE_PADDING);

  int close_x = contents_rect.right();
  if (showing_close_button_) {
    // If the ratio of the close button size to tab width exceeds the maximum.
    // The close button should be as large as possible so that there is a larger
    // hit-target for touch events. So the close button bounds extends to the
    // edges of the tab. However, the larger hit-target should be active only
    // for touch events, and the close-image should show up in the right place.
    // So a border is added to the button with necessary padding. The close
    // button (Tab::TabCloseButton) makes sure the padding is a hit-target only
    // for touch events.
    // TODO(pkasting): The padding should maybe be removed, see comments in
    // TabCloseButton::TargetForRect().
    close_button_->SetBorder(views::NullBorder());
    const gfx::Size close_button_size(close_button_->GetPreferredSize());
    const int top = contents_rect.y() +
                    Center(contents_rect.height(), close_button_size.height());
    // Clamp the close button position to "centered within the tab"; this should
    // only have an effect when animating in a new active tab, which might start
    // out narrower than the minimum active tab width.
    close_x = std::max(contents_rect.right() - close_button_size.width(),
                       Center(width(), close_button_size.width()));
    const int left = std::min(after_title_padding, close_x);
    close_button_->SetPosition(gfx::Point(close_x - left, 0));
    const int bottom = height() - close_button_size.height() - top;
    const int right =
        std::max(0, width() - (close_x + close_button_size.width()));
    close_button_->SetBorder(
        views::CreateEmptyBorder(top, left, bottom, right));
    close_button_->SizeToPreferredSize();
    // Re-layout the close button so it can recompute its focus ring if needed:
    // SizeToPreferredSize() will not necessarily re-Layout the View if only its
    // interior margins have changed (which this logic does), but the focus ring
    // still needs to be updated because it doesn't want to encompass the
    // interior margins.
    close_button_->Layout();
  }
  close_button_->SetVisible(showing_close_button_);

  if (showing_alert_indicator_) {
    const bool is_touch_optimized = MD::IsTouchOptimizedUiEnabled();
    const gfx::Size image_size(alert_indicator_button_->GetPreferredSize());
    int alert_to_close_spacing = 0;
    if (extra_alert_indicator_padding_) {
      alert_to_close_spacing =
          is_touch_optimized ? kTouchableRefreshAlertIndicatorCloseButtonPadding
                             : kRefreshAlertIndicatorCloseButtonPadding;
    } else if (!MD::IsRefreshUi() && is_touch_optimized) {
      alert_to_close_spacing = after_title_padding;
    }
    const int right = showing_close_button_ ? (close_x - alert_to_close_spacing)
                                            : contents_rect.right();
    gfx::Rect bounds(
        std::max(contents_rect.x(), right - image_size.width()),
        contents_rect.y() + Center(contents_rect.height(), image_size.height()),
        image_size.width(), image_size.height());
    MaybeAdjustLeftForPinnedTab(&bounds, bounds.width());
    alert_indicator_button_->SetBoundsRect(bounds);
  }
  alert_indicator_button_->SetVisible(showing_alert_indicator_);

  // Size the title to fill the remaining width and use all available height.
  bool show_title = ShouldRenderAsNormalTab();
  if (show_title) {
    int title_left = start;
    if (showing_icon_) {
      // When computing the spacing from the favicon, don't count the actual
      // icon view width (which will include extra room for the alert
      // indicator), but rather the normal favicon width which is what it will
      // look like.
      const int after_favicon = favicon_bounds.x() + gfx::kFaviconSize +
                                GetLayoutConstant(TAB_PRE_TITLE_PADDING);
      title_left = std::max(title_left, after_favicon);
    }
    int title_right = contents_rect.right();
    if (showing_alert_indicator_) {
      title_right = alert_indicator_button_->x() - after_title_padding;
    } else if (showing_close_button_) {
      // Allow the title to overlay the close button's empty border padding.
      title_right = close_x - after_title_padding;
    }
    const int title_width = std::max(title_right - title_left, 0);
    // The Label will automatically center the font's cap height within the
    // provided vertical space.
    const gfx::Rect title_bounds(title_left, contents_rect.y(), title_width,
                                 contents_rect.height());
    show_title = title_width > 0;

    if (title_bounds != target_title_bounds_) {
      target_title_bounds_ = title_bounds;
      if (was_showing_icon == showing_icon_ || title_->bounds().IsEmpty() ||
          title_bounds.IsEmpty()) {
        title_animation_.Stop();
        title_->SetBoundsRect(title_bounds);
      } else if (!title_animation_.is_animating()) {
        start_title_bounds_ = title_->bounds();
        title_animation_.Start();
      }
    }
  }
  title_->SetVisible(show_title);
}

const char* Tab::GetClassName() const {
  return kViewClassName;
}

namespace {
bool IsSelectionModifierDown(const ui::MouseEvent& event) {
#if defined(OS_MACOSX)
  return event.IsCommandDown();
#else
  return event.IsControlDown();
#endif
}
}  // namespace

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  controller_->OnMouseEventInTab(this, event);

  // Allow a right click from touch to drag, which corresponds to a long click.
  if (event.IsOnlyLeftMouseButton() ||
      (event.IsOnlyRightMouseButton() && event.flags() & ui::EF_FROM_TOUCH)) {
    ui::ListSelectionModel original_selection;
    original_selection = controller_->GetSelectionModel();
    // Changing the selection may cause our bounds to change. If that happens
    // the location of the event may no longer be valid. Create a copy of the
    // event in the parents coordinate, which won't change, and recreate an
    // event after changing so the coordinates are correct.
    ui::MouseEvent event_in_parent(event, static_cast<View*>(this), parent());
    if (controller_->SupportsMultipleSelection()) {
      if (event.IsShiftDown() && IsSelectionModifierDown(event)) {
        controller_->AddSelectionFromAnchorTo(this);
      } else if (event.IsShiftDown()) {
        controller_->ExtendSelectionTo(this);
      } else if (IsSelectionModifierDown(event)) {
        controller_->ToggleSelected(this);
        if (!IsSelected()) {
          // Don't allow dragging non-selected tabs.
          return false;
        }
      } else if (!IsSelected()) {
        controller_->SelectTab(this);
        base::RecordAction(UserMetricsAction("SwitchTab_Click"));
      }
    } else if (!IsSelected()) {
      controller_->SelectTab(this);
      base::RecordAction(UserMetricsAction("SwitchTab_Click"));
    }
    ui::MouseEvent cloned_event(event_in_parent, parent(),
                                static_cast<View*>(this));
    controller_->MaybeStartDrag(this, cloned_event, original_selection);
  }
  return true;
}

bool Tab::OnMouseDragged(const ui::MouseEvent& event) {
  controller_->ContinueDrag(this, event);
  return true;
}

void Tab::OnMouseReleased(const ui::MouseEvent& event) {
  controller_->OnMouseEventInTab(this, event);

  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (controller_->EndDrag(END_DRAG_COMPLETE))
    return;

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsMiddleMouseButton()) {
    if (HitTestPoint(event.location())) {
      controller_->CloseTab(this, CLOSE_TAB_FROM_MOUSE);
    } else if (closing_) {
      // We're animating closed and a middle mouse button was pushed on us but
      // we don't contain the mouse anymore. We assume the user is clicking
      // quicker than the animation and we should close the tab that falls under
      // the mouse.
      gfx::Point location_in_parent = event.location();
      ConvertPointToTarget(this, parent(), &location_in_parent);
      Tab* closest_tab = controller_->GetTabAt(location_in_parent);
      if (closest_tab)
        controller_->CloseTab(closest_tab, CLOSE_TAB_FROM_MOUSE);
    }
  } else if (event.IsOnlyLeftMouseButton() && !event.IsShiftDown() &&
             !IsSelectionModifierDown(event)) {
    // If the tab was already selected mouse pressed doesn't change the
    // selection. Reset it now to handle the case where multiple tabs were
    // selected.
    controller_->SelectTab(this);

    if (alert_indicator_button_ && alert_indicator_button_->visible() &&
        alert_indicator_button_->bounds().Contains(event.location())) {
      base::RecordAction(UserMetricsAction("TabAlertIndicator_Clicked"));
    }
  }
}

void Tab::OnMouseCaptureLost() {
  controller_->EndDrag(END_DRAG_CAPTURE_LOST);
}

void Tab::OnMouseMoved(const ui::MouseEvent& event) {
  hover_controller_.SetLocation(event.location());
  controller_->OnMouseEventInTab(this, event);
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_hovered_ = true;
  hover_controller_.SetSubtleOpacityScale(
      controller_->GetHoverOpacityForRadialHighlight());
  hover_controller_.Show(GlowHoverController::SUBTLE);
  Layout();
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  mouse_hovered_ = false;
  hover_controller_.Hide();
  Layout();
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      // TAP_DOWN is only dispatched for the first touch point.
      DCHECK_EQ(1, event->details().touch_points());

      // See comment in OnMousePressed() as to why we copy the event.
      ui::GestureEvent event_in_parent(*event, static_cast<View*>(this),
                                       parent());
      ui::ListSelectionModel original_selection;
      original_selection = controller_->GetSelectionModel();
      tab_activated_with_last_tap_down_ = !IsActive();
      if (!IsSelected())
        controller_->SelectTab(this);
      gfx::Point loc(event->location());
      views::View::ConvertPointToScreen(this, &loc);
      ui::GestureEvent cloned_event(event_in_parent, parent(),
                                    static_cast<View*>(this));
      controller_->MaybeStartDrag(this, cloned_event, original_selection);
      break;
    }

    case ui::ET_GESTURE_END:
      controller_->EndDrag(END_DRAG_COMPLETE);
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      controller_->ContinueDrag(this, *event);
      break;

    default:
      break;
  }
  event->SetHandled();
}

bool Tab::GetTooltipText(const gfx::Point& p, base::string16* tooltip) const {
  // Note: Anything that affects the tooltip text should be accounted for when
  // calling TooltipTextChanged() from Tab::SetData().
  *tooltip = chrome::AssembleTabTooltipText(data_.title, data_.alert_state);
  return !tooltip->empty();
}

bool Tab::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* origin) const {
  origin->set_x(title_->x() + 10);
  origin->set_y(-4);
  return true;
}

void Tab::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTab;
  node_data->SetName(controller_->GetAccessibleTabName(this));
  node_data->AddState(ax::mojom::State::kMultiselectable);
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              IsSelected());
}

gfx::Size Tab::CalculatePreferredSize() const {
  return gfx::Size(GetStandardWidth(), GetLayoutConstant(TAB_HEIGHT));
}

void Tab::PaintChildren(const views::PaintInfo& info) {
  // Clip children based on the tab's fill path.  This has no effect except when
  // the tab is too narrow to completely show even one icon, at which point this
  // serves to clip the favicon.
  ui::ClipRecorder clip_recorder(info.context());
  // The paint recording scale for tabs is consistent along the x and y axis.
  const float paint_recording_scale = info.paint_recording_scale_x();
  // When there is a separator, animate the clip to account for it, in sync with
  // the separator's fading.
  // TODO(pkasting): Consider crossfading the favicon instead of animating the
  // clip, especially if other children get crossfaded.
  const auto opacities = GetSeparatorOpacities(true);
  constexpr float kChildClipPadding = 2.5f;
  const gfx::InsetsF padding(0, kChildClipPadding + opacities.left, 0,
                             kChildClipPadding + opacities.right);
  clip_recorder.ClipPathWithAntiAliasing(GetInteriorPath(
      paint_recording_scale, paint_recording_scale * GetStrokeThickness(),
      IsActive() ? 0 : controller_->GetStrokeThickness(), bounds(), padding));
  View::PaintChildren(info);
}

void Tab::OnPaint(gfx::Canvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (!MD::IsRefreshUi() && (width() < GetMinimumInactiveWidth()))
    return;

  gfx::Path clip;
  if (!controller_->ShouldPaintTab(
          this,
          base::BindRepeating(
              &GetBorderPath, canvas->image_scale(), GetStrokeThickness(),
              IsActive() ? 0 : controller_->GetStrokeThickness(), true, false),
          &clip))
    return;

  PaintTab(canvas, clip);
}

void Tab::AddedToWidget() {
  OnButtonColorMaybeChanged();
}

void Tab::OnThemeChanged() {
  OnButtonColorMaybeChanged();
}

void Tab::SetClosing(bool closing) {
  closing_ = closing;
  ActiveStateChanged();
}

SkColor Tab::GetAlertIndicatorColor(TabAlertState state) const {
  const bool is_touch_optimized = MD::IsTouchOptimizedUiEnabled();
  // If theme provider is not yet available, return the default button
  // color.
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return button_color_;

  switch (state) {
    case TabAlertState::AUDIO_PLAYING:
    case TabAlertState::AUDIO_MUTING:
      return is_touch_optimized ? theme_provider->GetColor(
                                      ThemeProperties::COLOR_TAB_ALERT_AUDIO)
                                : button_color_;
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::DESKTOP_CAPTURING:
      return theme_provider->GetColor(
          ThemeProperties::COLOR_TAB_ALERT_RECORDING);
    case TabAlertState::TAB_CAPTURING:
      return is_touch_optimized
                 ? theme_provider->GetColor(
                       ThemeProperties::COLOR_TAB_ALERT_CAPTURING)
                 : button_color_;
    case TabAlertState::PIP_PLAYING:
      return theme_provider->GetColor(ThemeProperties::COLOR_TAB_PIP_PLAYING);
    case TabAlertState::BLUETOOTH_CONNECTED:
    case TabAlertState::USB_CONNECTED:
    case TabAlertState::NONE:
      return button_color_;
    default:
      NOTREACHED();
      return button_color_;
  }
}

SkColor Tab::GetCloseTabButtonColor(
    views::Button::ButtonState button_state) const {
  // The theme provider may be null if we're not currently in a widget
  // hierarchy.
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return SK_ColorTRANSPARENT;

  int color_id;
  switch (button_state) {
    case views::Button::STATE_HOVERED:
      color_id = ThemeProperties::COLOR_TAB_CLOSE_BUTTON_BACKGROUND_HOVER;
      break;
    case views::Button::STATE_PRESSED:
      color_id = ThemeProperties::COLOR_TAB_CLOSE_BUTTON_BACKGROUND_PRESSED;
      break;
    default:
      color_id = IsActive() ? ThemeProperties::COLOR_TAB_CLOSE_BUTTON_ACTIVE
                            : ThemeProperties::COLOR_TAB_CLOSE_BUTTON_INACTIVE;
  }
  return theme_provider->GetColor(color_id);
}

bool Tab::IsActive() const {
  return controller_->IsActiveTab(this);
}

void Tab::ActiveStateChanged() {
  UpdateTabIconNeedsAttentionBlocked();
  OnButtonColorMaybeChanged();
  alert_indicator_button_->UpdateEnabledForMuteToggle();
  Layout();
}

void Tab::AlertStateChanged() {
  Layout();
}

void Tab::FrameColorsChanged() {
  OnButtonColorMaybeChanged();
  SchedulePaint();
}

void Tab::SelectedStateChanged() {
  OnButtonColorMaybeChanged();
}

bool Tab::IsSelected() const {
  return controller_->IsTabSelected(this);
}

void Tab::SetData(TabRendererData data) {
  DCHECK(GetWidget());

  if (data_ == data)
    return;

  TabRendererData old(std::move(data_));
  data_ = std::move(data);

  // Icon updating must be done first because the title depends on whether the
  // loading animation is showing.
  icon_->SetIcon(data_.url, data_.favicon);
  icon_->SetNetworkState(data_.network_state, data_.should_hide_throbber);
  icon_->SetCanPaintToLayer(controller_->CanPaintThrobberToLayer());
  icon_->SetIsCrashed(data_.IsCrashed());
  UpdateTabIconNeedsAttentionBlocked();

  base::string16 title = data_.title;
  if (title.empty()) {
    title = icon_->ShowingLoadingAnimation()
                ? l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE)
                : CoreTabHelper::GetDefaultTitle();
  } else {
    Browser::FormatTitleForDisplay(&title);
  }
  title_->SetText(title);

  if (data_.alert_state != old.alert_state)
    alert_indicator_button_->TransitionToAlertState(data_.alert_state);
  if (old.pinned != data_.pinned)
    showing_alert_indicator_ = false;

  if (data_.alert_state != old.alert_state || data_.title != old.title)
    TooltipTextChanged();

  Layout();
  SchedulePaint();
}

void Tab::StepLoadingAnimation() {
  icon_->StepLoadingAnimation();

  // Update the layering if necessary.
  //
  // TODO(brettw) this design should be changed to be a push state when the tab
  // can't be painted to a layer, rather than continually polling the
  // controller about the state and reevaulating that state in the icon. This
  // is both overly aggressive and wasteful in the common case, and not
  // frequent enough in other cases since the state can be updated and the tab
  // painted before the animation is stepped.
  icon_->SetCanPaintToLayer(controller_->CanPaintThrobberToLayer());
}

void Tab::StartPulse() {
  pulse_animation_.StartThrobbing(std::numeric_limits<int>::max());
}

void Tab::StopPulse() {
  pulse_animation_.Stop();
}

void Tab::SetTabNeedsAttention(bool attention) {
  icon_->SetAttention(TabIcon::AttentionType::kTabWantsAttentionStatus,
                      attention);
  SchedulePaint();
}

float Tab::GetStrokeThickness(bool should_paint_as_active) const {
  return !MD::IsRefreshUi() || IsActive() || should_paint_as_active
             ? controller_->GetStrokeThickness()
             : 0;
}

int Tab::GetWidthOfLargestSelectableRegion() const {
  // Assume the entire region to the left of the alert indicator and/or close
  // buttons is available for click-to-select.  If neither are visible, the
  // entire tab region is available.
  const int indicator_left = alert_indicator_button_->visible()
                                 ? alert_indicator_button_->x()
                                 : width();
  const int close_button_left =
      close_button_->visible() ? close_button_->x() : width();
  return std::min(indicator_left, close_button_left);
}

gfx::Insets Tab::GetContentsInsets() const {
  const float stroke_thickness = GetStrokeThickness();
  return GetContentsHorizontalInsets() +
         gfx::Insets(
             stroke_thickness, 0,
             stroke_thickness + GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP), 0);
}

// static
gfx::Insets Tab::GetContentsHorizontalInsets() {
  return gfx::Insets(0, MD::IsRefreshUi() ? (GetCornerRadius() * 2)
                                          : GetTabEndcapWidthForLayout());
}

// static
int Tab::GetMinimumInactiveWidth() {
  if (!MD::IsRefreshUi())
    return GetContentsHorizontalInsets().width();

  // Allow tabs to shrink until they appear to be 16 DIP wide excluding outer
  // corners.
  constexpr int kInteriorWidth = 16;
  // The overlap contains the trailing separator that is part of the interior
  // width; avoid double-counting it.
  return kInteriorWidth - kSeparatorThickness + GetOverlap();
}

// static
int Tab::GetMinimumActiveWidth() {
  return TabCloseButton::GetWidth() + GetContentsHorizontalInsets().width();
}

// static
int Tab::GetStandardWidth() {
  constexpr int kRefreshTabWidth = 240 - kSeparatorThickness;
  constexpr int kLayoutWidth[] = {193, 193, 245, kRefreshTabWidth,
                                  kRefreshTabWidth};
  return GetOverlap() + kLayoutWidth[MD::GetMode()];
}

// static
int Tab::GetPinnedWidth() {
  constexpr int kTabPinnedContentWidth = 23;
  return kTabPinnedContentWidth + GetContentsHorizontalInsets().width();
}

// static
float Tab::GetInverseDiagonalSlope() {
  // In refresh, tab sides do not have slopes, so no one should call this.
  DCHECK(!MD::IsRefreshUi());

  // This is computed from the border path as follows:
  // * The endcap width is enough for the whole stroke outer curve, i.e. the
  //   side diagonal plus the curves on both its ends.
  // * The bottom and top curve together are 4 DIP wide, so the diagonal is
  //   (endcap width - 4) DIP wide.
  // * The bottom and top curve are each 1.5 px high.  Additionally, there is an
  //   extra 1 px below the bottom curve and (scale - 1) px above the top curve,
  //   so the diagonal is ((height - 1.5 - 1.5) * scale - 1 - (scale - 1)) px
  //   high.  Simplifying this gives (height - 4) * scale px, or (height - 4)
  //   DIP.
  return (GetTabEndcapWidthForPainting() - 4) /
         (GetLayoutConstant(TAB_HEIGHT) - 4);
}

// static
int Tab::GetCornerRadius() {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_HIGH);
}

// static
int Tab::GetDragInset() {
  return MD::IsRefreshUi() ? GetCornerRadius() : GetTabEndcapWidthForLayout();
}

// static
int Tab::GetOverlap() {
  // For refresh, overlap the separators.
  return MD::IsRefreshUi() ? (GetCornerRadius() * 2 + kSeparatorThickness)
                           : GetTabEndcapWidthForLayout();
}

// static
int Tab::GetTabSeparatorHeight() {
  return MD::IsTouchOptimizedUiEnabled() ? kTabSeparatorTouchHeight
                                         : kTabSeparatorHeight;
}

void Tab::MaybeAdjustLeftForPinnedTab(gfx::Rect* bounds,
                                      int visual_width) const {
  if (ShouldRenderAsNormalTab())
    return;
  const int pinned_width = GetPinnedWidth();
  const int ideal_delta = width() - pinned_width;
  const int ideal_x = (pinned_width - visual_width) / 2;
  // TODO(pkasting): https://crbug.com/533570  This code is broken when the
  // current width is less than the pinned width.
  bounds->set_x(
      bounds->x() +
      gfx::ToRoundedInt(
          (1 - static_cast<float>(ideal_delta) /
                   static_cast<float>(kPinnedTabExtraWidthToRenderAsNormal)) *
          (ideal_x - bounds->x())));
}

void Tab::PaintTab(gfx::Canvas* canvas, const gfx::Path& clip) {
  int active_tab_fill_id = 0;
  int active_tab_y_inset = 0;
  if (GetThemeProvider()->HasCustomImage(IDR_THEME_TOOLBAR)) {
    active_tab_fill_id = IDR_THEME_TOOLBAR;
    active_tab_y_inset = GetStrokeThickness(true);
  }

  if (IsActive()) {
    PaintTabBackground(canvas, true /* active */, active_tab_fill_id,
                       active_tab_y_inset, nullptr /* clip */);
  } else {
    PaintInactiveTabBackground(canvas, clip);

    const float throb_value = GetThrobValue();
    if (throb_value > 0) {
      canvas->SaveLayerAlpha(gfx::ToRoundedInt(throb_value * 0xff),
                             GetLocalBounds());
      PaintTabBackground(canvas, true /* active */, active_tab_fill_id,
                         active_tab_y_inset, nullptr /* clip */);
      canvas->Restore();
    }
  }
}

void Tab::PaintInactiveTabBackground(gfx::Canvas* canvas,
                                     const gfx::Path& clip) {
  bool has_custom_image;
  int fill_id = controller_->GetBackgroundResourceId(&has_custom_image);
  if (!has_custom_image)
    fill_id = 0;

  PaintTabBackground(canvas, false /* active */, fill_id, 0,
                     controller_->MaySetClip() ? &clip : nullptr);
}

void Tab::PaintTabBackground(gfx::Canvas* canvas,
                             bool active,
                             int fill_id,
                             int y_inset,
                             const gfx::Path* clip) {
  // |y_inset| is only set when |fill_id| is being used.
  DCHECK(!y_inset || fill_id);

  const SkColor active_color = controller_->GetTabBackgroundColor(TAB_ACTIVE);
  const SkColor inactive_color =
      GetThemeProvider()->GetDisplayProperty(
          ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR)
          ? controller_->GetTabBackgroundColor(TAB_INACTIVE)
          : SK_ColorTRANSPARENT;
  const SkColor stroke_color = controller_->GetToolbarTopSeparatorColor();
  const bool paint_hover_effect = !active && hover_controller_.ShouldDraw();

  // If there is a |fill_id| we don't try to cache. This could be improved
  // but would require knowing then the image from the ThemeProvider had been
  // changed, and invalidating when the tab's x-coordinate or background_offset_
  // changed.
  //
  // If |paint_hover_effect|, we don't try to cache since hover effects change
  // on every invalidation and we would need to invalidate the cache based on
  // the hover states.
  //
  // Finally, in refresh, we don't cache for non-integral scale factors, since
  // tabs draw with slightly different offsets so as to pixel-align the layout
  // rect (see ScaleAndAlignBounds()).
  const float scale = canvas->image_scale();
  const float stroke_thickness = GetStrokeThickness(active);
  const float bottom_offset = active ? 0 : controller_->GetStrokeThickness();
  if (fill_id || paint_hover_effect ||
      (MD::IsRefreshUi() && (std::trunc(scale) != scale))) {
    // When there's a border, we want the stroke to cover up the edge of the
    // fill path (https://crbug.com/873003), so set the fill path halfway
    // between the inner path and the border paths.  When there's no stroke,
    // |stroke_thickness| is 0 and the fill, inner, and stroke paths are all
    // identical.  Avoid doing this on pre-refresh since strokes may be
    // transparent.
    gfx::Path fill_path = GetInteriorPath(
        scale, MD::IsRefreshUi() ? stroke_thickness / 2 : stroke_thickness,
        bottom_offset, bounds());
    PaintTabBackgroundFill(canvas, fill_path, active, paint_hover_effect,
                           active_color, inactive_color, fill_id, y_inset);
    if (stroke_thickness > 0) {
      gfx::Path interior_path =
          GetInteriorPath(scale, stroke_thickness, bottom_offset, bounds());
      gfx::Path outer_path = GetBorderPath(
          scale, stroke_thickness, bottom_offset, false, false, bounds());
      gfx::ScopedCanvas scoped_canvas(clip ? canvas : nullptr);
      if (clip)
        canvas->sk_canvas()->clipPath(*clip, SkClipOp::kDifference, true);
      PaintTabBackgroundStroke(canvas, interior_path, outer_path, active,
                               stroke_color);
    }
  } else {
    BackgroundCache& cache =
        active ? background_active_cache_ : background_inactive_cache_;
    if (!cache.CacheKeyMatches(scale, size(), active_color, inactive_color,
                               stroke_color, stroke_thickness)) {
      // See the comment in the non-caching case above for why we divide
      // |stroke_thickness| by 2 on refresh.
      gfx::Path fill_path = GetInteriorPath(
          scale, MD::IsRefreshUi() ? stroke_thickness / 2 : stroke_thickness,
          bottom_offset, bounds());
      cc::PaintRecorder recorder;

      {
        gfx::Canvas cache_canvas(
            recorder.beginRecording(size().width(), size().height()), scale);
        PaintTabBackgroundFill(&cache_canvas, fill_path, active,
                               paint_hover_effect, active_color, inactive_color,
                               fill_id, y_inset);
        cache.fill_record = recorder.finishRecordingAsPicture();
      }
      if (stroke_thickness > 0) {
        gfx::Path interior_path =
            GetInteriorPath(scale, stroke_thickness, bottom_offset, bounds());
        gfx::Path border_path = GetBorderPath(
            scale, stroke_thickness, bottom_offset, false, false, bounds());
        gfx::Canvas cache_canvas(
            recorder.beginRecording(size().width(), size().height()), scale);
        PaintTabBackgroundStroke(&cache_canvas, interior_path, border_path,
                                 active, stroke_color);
        cache.stroke_record = recorder.finishRecordingAsPicture();
      }

      cache.SetCacheKey(scale, size(), active_color, inactive_color,
                        stroke_color, stroke_thickness);
    }

    canvas->sk_canvas()->drawPicture(cache.fill_record);
    if (stroke_thickness > 0) {
      gfx::ScopedCanvas scoped_canvas(clip ? canvas : nullptr);
      if (clip)
        canvas->sk_canvas()->clipPath(*clip, SkClipOp::kDifference, true);
      canvas->sk_canvas()->drawPicture(cache.stroke_record);
    }
  }

  PaintSeparators(canvas);
}

void Tab::PaintTabBackgroundFill(gfx::Canvas* canvas,
                                 const gfx::Path& fill_path,
                                 bool active,
                                 bool paint_hover_effect,
                                 SkColor active_color,
                                 SkColor inactive_color,
                                 int fill_id,
                                 int y_inset) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  canvas->ClipPath(fill_path, true);

  // In the active case, always fill the tab with its bg color first in case the
  // image is transparent. In the inactive case, the image is guaranteed to be
  // opaque, so it's only necessary to fill the color when there's no image.
  if (active || !fill_id) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(active ? active_color : inactive_color);
    canvas->DrawRect(gfx::ScaleToEnclosingRect(GetLocalBounds(), scale), flags);
  }

  if (fill_id) {
    gfx::ScopedCanvas scale_scoper(canvas);
    canvas->sk_canvas()->scale(scale, scale);
    canvas->TileImageInt(*GetThemeProvider()->GetImageSkiaNamed(fill_id),
                         GetMirroredX() + background_offset_, 0, 0, y_inset,
                         width(), height());
  }

  if (paint_hover_effect) {
    SkPoint hover_location(gfx::PointToSkPoint(hover_controller_.location()));
    hover_location.scale(SkFloatToScalar(scale));
    const SkScalar kMinHoverRadius = 16;
    const SkScalar radius =
        std::max(SkFloatToScalar(width() / 4.f), kMinHoverRadius);
    DrawHighlight(canvas, hover_location, radius * scale,
                  SkColorSetA(active_color, hover_controller_.GetAlpha()));
  }
}

void Tab::PaintTabBackgroundStroke(gfx::Canvas* canvas,
                                   const gfx::Path& fill_path,
                                   const gfx::Path& stroke_path,
                                   bool active,
                                   SkColor color) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  if (!active) {
    // Clip out the bottom line; this will be drawn for us by
    // TabStrip::PaintChildren().
    canvas->ClipRect(gfx::RectF(width() * scale, height() * scale - 1));
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  SkPath path;
  Op(stroke_path, fill_path, kDifference_SkPathOp, &path);
  canvas->DrawPath(path, flags);
}

void Tab::PaintSeparators(gfx::Canvas* canvas) {
  const auto separator_opacities = GetSeparatorOpacities(false);
  if (!separator_opacities.left && !separator_opacities.right)
    return;

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  const gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(bounds(), scale, GetStrokeThickness());
  const int corner_radius = GetCornerRadius();
  const float separator_height = GetTabSeparatorHeight() * scale;
  gfx::RectF leading_separator_bounds(
      aligned_bounds.x() + corner_radius * scale,
      aligned_bounds.y() + (aligned_bounds.height() - separator_height) / 2,
      kSeparatorThickness * scale, separator_height);
  gfx::RectF trailing_separator_bounds = leading_separator_bounds;
  trailing_separator_bounds.set_x(
      aligned_bounds.right() - (corner_radius + kSeparatorThickness) * scale);

  gfx::PointF origin(bounds().origin());
  origin.Scale(scale);
  leading_separator_bounds.Offset(-origin.x(), -origin.y());
  trailing_separator_bounds.Offset(-origin.x(), -origin.y());

  const SkColor separator_base_color = controller_->GetTabSeparatorColor();
  const auto separator_color = [separator_base_color](float opacity) {
    return SkColorSetA(separator_base_color,
                       gfx::Tween::IntValueBetween(opacity, SK_AlphaTRANSPARENT,
                                                   SK_AlphaOPAQUE));
  };

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(separator_color(separator_opacities.left));
  canvas->DrawRect(leading_separator_bounds, flags);
  flags.setColor(separator_color(separator_opacities.right));
  canvas->DrawRect(trailing_separator_bounds, flags);
}

void Tab::UpdateIconVisibility() {
  // TODO(pkasting): This whole function should go away, and we should simply
  // compute child visibility state in Layout().

  // Don't adjust whether we're centering the favicon or adding extra padding
  // during tab closure; let it stay however it was prior to closing the tab.
  // This prevents the icon and text from sliding left at the end of closing
  // a non-narrow tab.
  if (!closing_) {
    center_favicon_ = false;
    extra_padding_before_content_ = false;
  }

  showing_icon_ = showing_alert_indicator_ = false;
  extra_alert_indicator_padding_ = false;

  if (height() < GetLayoutConstant(TAB_HEIGHT))
    return;

  const bool has_favicon = data().show_icon;
  const bool has_alert_icon =
      (alert_indicator_button_ ? alert_indicator_button_->showing_alert_state()
                               : data().alert_state) != TabAlertState::NONE;

  if (data().pinned) {
    // When the tab is pinned, we can show one of the two icons; the alert icon
    // is given priority over the favicon. The close buton is never shown.
    showing_alert_indicator_ = has_alert_icon;
    showing_icon_ = has_favicon && !has_alert_icon;
    showing_close_button_ = false;
    return;
  }

  int available_width = GetContentsBounds().width();

  const bool is_touch_optimized = MD::IsTouchOptimizedUiEnabled();
  const int favicon_width = gfx::kFaviconSize;
  const int alert_icon_width =
      alert_indicator_button_->GetPreferredSize().width();
  // In case of touch optimized UI, the close button has an extra padding on the
  // left that needs to be considered.
  const int close_button_width =
      close_button_->GetPreferredSize().width() -
      (is_touch_optimized ? close_button_->GetInsets().right()
                          : close_button_->GetInsets().width());
  const bool large_enough_for_close_button =
      available_width >= (is_touch_optimized
                              ? kTouchableMinimumContentsWidthForCloseButtons
                              : kMinimumContentsWidthForCloseButtons);

  showing_close_button_ = !controller_->ShouldHideCloseButtonForTab(this);
  if (IsActive()) {
    // Close button is shown on active tabs regardless of the size.
    if (showing_close_button_)
      available_width -= close_button_width;

    showing_alert_indicator_ =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator_)
      available_width -= alert_icon_width;

    showing_icon_ = has_favicon && favicon_width <= available_width;
    if (showing_icon_)
      available_width -= favicon_width;
  } else {
    showing_alert_indicator_ =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator_)
      available_width -= alert_icon_width;

    showing_icon_ = has_favicon && favicon_width <= available_width;
    if (showing_icon_)
      available_width -= favicon_width;

    // Show the close button if it's allowed to show on hover, even if it's
    // forced to be hidden normally.
    const bool show_on_hover = controller_->ShouldShowCloseButtonOnHover();
    showing_close_button_ |= show_on_hover && hover_controller_.ShouldDraw();
    showing_close_button_ &= large_enough_for_close_button;
    if (showing_close_button_ || show_on_hover)
      available_width -= close_button_width;

    // If no other controls are visible, show favicon even though we
    // don't have enough space. We'll clip the favicon in PaintChildren().
    if (!showing_close_button_ && !showing_alert_indicator_ && !showing_icon_ &&
        has_favicon) {
      showing_icon_ = true;

      // See comments near top of function on why this conditional is here.
      if (!closing_)
        center_favicon_ = true;
    }
  }

  // Don't update padding while the tab is closing, to avoid glitchy-looking
  // behaviour when the close animation causes the tab to get very small
  if (!closing_) {
    // The extra padding is intended to visually balance the close button, so
    // only include it when the close button is shown or will be shown on hover.
    // We also check this for active tabs so that the extra padding doesn't pop
    // in and out as you switch tabs.
    extra_padding_before_content_ = large_enough_for_close_button;

    if (DCHECK_IS_ON()) {
      const int extra_left_padding =
          MD::IsRefreshUi()
              ? kRefreshExtraLeftPaddingToBalanceCloseButtonPadding
              : kExtraLeftPaddingToBalanceCloseButtonPadding;
      DCHECK(!extra_padding_before_content_ ||
             extra_left_padding <= available_width);
      if (extra_padding_before_content_)
        available_width -= extra_left_padding;
    }
  }

  if (MD::IsRefreshUi()) {
    extra_alert_indicator_padding_ = showing_alert_indicator_ &&
                                     showing_close_button_ &&
                                     large_enough_for_close_button;

    if (DCHECK_IS_ON()) {
      const int extra_alert_padding =
          MD::IsTouchOptimizedUiEnabled()
              ? kTouchableRefreshAlertIndicatorCloseButtonPadding
              : kRefreshAlertIndicatorCloseButtonPadding;
      DCHECK(!extra_alert_indicator_padding_ ||
             extra_alert_padding <= available_width);
    }
  }
}

bool Tab::ShouldRenderAsNormalTab() const {
  return !data().pinned ||
      (width() >= (GetPinnedWidth() + kPinnedTabExtraWidthToRenderAsNormal));
}

Tab::SeparatorOpacities Tab::GetSeparatorOpacities(bool for_layout) const {
  if (!MD::IsRefreshUi())
    return SeparatorOpacities();

  // Something should visually separate tabs from each other and any adjacent
  // new tab button.  Normally, active and hovered tabs draw distinct shapes
  // (via different background colors) and thus need no separators, while
  // background tabs need separators between them.  In single-tab mode, the
  // active tab has no visible shape and thus needs separators on any side with
  // an adjacent new tab button.  (The other sides will be faded out below.)
  float leading_opacity, trailing_opacity;
  if (controller_->SingleTabMode()) {
    leading_opacity = trailing_opacity = 1.f;
  } else if (IsActive()) {
    leading_opacity = trailing_opacity = 0;
  } else {
    // Fade out the trailing separator while this tab or the subsequent tab is
    // hovered.  If the subsequent tab is active, don't consider its hover
    // animation value, lest the trailing separator on this tab disappear while
    // the subsequent tab is being dragged.
    const float hover_value = hover_controller_.GetAnimationValue();
    const Tab* subsequent_tab = controller_->GetAdjacentTab(this, 1);
    const float subsequent_hover =
        !for_layout && subsequent_tab && !subsequent_tab->IsActive()
            ? float{subsequent_tab->hover_controller_.GetAnimationValue()}
            : 0;
    trailing_opacity = 1.f - std::max(hover_value, subsequent_hover);

    // The leading separator need not consider the previous tab's hover value,
    // since if there is a previous tab that's hovered and not being dragged, it
    // will draw atop this tab.
    leading_opacity = 1.f - hover_value;

    const Tab* previous_tab = controller_->GetAdjacentTab(this, -1);
    if (IsSelected()) {
      // Since this tab is selected, its shape will be visible against adjacent
      // unselected tabs, so remove the separator in those cases.
      if (previous_tab && !previous_tab->IsSelected())
        leading_opacity = 0;
      if (subsequent_tab && !subsequent_tab->IsSelected())
        trailing_opacity = 0;
    } else if (controller_->HasVisibleBackgroundTabShapes()) {
      // Since this tab is unselected, adjacent selected tabs will normally
      // paint atop it, covering the separator.  But if the user drags those
      // selected tabs away, the exposed region looks like the window frame; and
      // since background tab shapes are visible, there should be no separator.
      // TODO(pkasting): https://crbug.com/876599  When a tab is animating into
      // this gap, we should adjust its separator opacities as well.
      if (previous_tab && previous_tab->IsSelected())
        leading_opacity = 0;
      if (subsequent_tab && subsequent_tab->IsSelected())
        trailing_opacity = 0;
    }
  }

  // For the first or last tab in the strip, fade the leading or trailing
  // separator based on the NTB position and how close to the target bounds this
  // tab is.  In the steady state, this hides separators on the opposite end of
  // the strip from the NTB; it fades out the separators as tabs animate into
  // these positions, after they pass by the other tabs; and it snaps the
  // separators to full visibility immediately when animating away from these
  // positions, which seems desirable.
  const NewTabButtonPosition ntb_position =
      controller_->GetNewTabButtonPosition();
  const gfx::Rect target_bounds =
      controller_->GetTabAnimationTargetBounds(this);
  const int tab_width = std::max(width(), target_bounds.width());
  const float target_opacity =
      float{std::min(std::abs(x() - target_bounds.x()), tab_width)} / tab_width;
  // If the tab shapes are visible, never draw end separators.
  const bool always_hide_separators_on_ends =
      controller_->HasVisibleBackgroundTabShapes();
  if (controller_->IsFirstVisibleTab(this) &&
      (ntb_position != LEADING || always_hide_separators_on_ends))
    leading_opacity = target_opacity;
  if (controller_->IsLastVisibleTab(this) &&
      (ntb_position != AFTER_TABS || always_hide_separators_on_ends))
    trailing_opacity = target_opacity;

  // Return the opacities in physical order, rather than logical.
  if (base::i18n::IsRTL())
    std::swap(leading_opacity, trailing_opacity);
  return {leading_opacity, trailing_opacity};
}

float Tab::GetThrobValue() const {
  const bool is_selected = IsSelected();
  double val = is_selected ? kSelectedTabOpacity : 0;

  // Wrapping in closure to only compute offset when needed (animate or hover).
  const auto offset = [=] {
    // Opacity boost varies on tab width.  The interpolation is nonlinear so
    // that most tabs will fall on the low end of the opacity range, but very
    // narrow tabs will still stand out on the high end.
    const float range_start = float{GetStandardWidth()};
    const float range_end = float{GetMinimumInactiveWidth()};
    const float value_in_range = float{width()};
    const float t = (value_in_range - range_start) / (range_end - range_start);
    const float opacity = controller_->GetHoverOpacityForTab(t * t);
    return is_selected ? (kSelectedTabThrobScale * opacity) : opacity;
  };

  if (pulse_animation_.is_animating())
    val += pulse_animation_.GetCurrentValue() * offset();
  else if (hover_controller_.ShouldDraw())
    val += hover_controller_.GetAnimationValue() * offset();

  return val;
}

void Tab::OnButtonColorMaybeChanged() {
  // The theme provider may be null if we're not currently in a widget
  // hierarchy.
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;

  UpdateButtonIconColors(controller_->GetTabForegroundColor(
      IsActive() ? TAB_ACTIVE : TAB_INACTIVE));
}

void Tab::UpdateTabIconNeedsAttentionBlocked() {
  // Only show the blocked attention indicator on non-active tabs. For active
  // tabs, the user sees the dialog blocking the tab, so there's no point to it
  // and it would be distracting.
  if (IsActive()) {
    icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents, false);
  } else {
    icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents,
                        data_.blocked);
  }
}

void Tab::UpdateButtonIconColors(SkColor title_color) {
  // These ratios are calculated from the default colors specified in the
  // Material Refresh design document. Active/inactive are the contrast ratios
  // of the close X against the tab background. Hovered/pressed are the contrast
  // ratios of the highlight circle against the tab background.
  constexpr float kMinimumActiveContrastRatio = 6.05f;
  constexpr float kMinimumInactiveContrastRatio = 4.61f;
  constexpr float kMinimumHoveredContrastRatio = 5.02f;
  constexpr float kMinimumPressedContrastRatio = 4.41f;

  SkColor tab_bg_color = controller_->GetTabBackgroundColor(
      IsActive() ? TAB_ACTIVE : TAB_INACTIVE);
  SkColor tab_title_color = title_color;

  // Make sure the text has enough contrast to be visible if this tab is
  // selected.
  if (IsSelected() && !IsActive()) {
    const SkColor tab_active_bg_color =
        controller_->GetTabBackgroundColor(TAB_ACTIVE);
    tab_bg_color = color_utils::AlphaBlend(
        tab_active_bg_color, tab_bg_color,
        gfx::ToRoundedInt(kSelectedTabOpacity * SK_AlphaOPAQUE));
    tab_title_color =
        color_utils::GetColorWithMinimumContrast(title_color, tab_bg_color);
  }

  title_->SetEnabledColor(tab_title_color);

  const SkColor base_icon_color =
      MD::GetMode() == ui::MaterialDesignController::MATERIAL_TOUCH_OPTIMIZED
          ? GetCloseTabButtonColor(views::Button::STATE_NORMAL)
          : tab_title_color;
  const SkColor base_hovered_pressed_icon_color =
      MD::IsNewerMaterialUi() ? base_icon_color : SK_ColorWHITE;
  const SkColor base_hovered_color =
      GetCloseTabButtonColor(views::Button::STATE_HOVERED);
  const SkColor base_pressed_color =
      GetCloseTabButtonColor(views::Button::STATE_PRESSED);

  const auto get_color_for_contrast_ratio = [](SkColor fg_color,
                                               SkColor bg_color,
                                               float contrast_ratio) {
    const SkAlpha blend_alpha = color_utils::GetBlendValueWithMinimumContrast(
        bg_color, fg_color, bg_color, contrast_ratio);
    return color_utils::AlphaBlend(fg_color, bg_color, blend_alpha);
  };

  const SkColor generated_icon_color = get_color_for_contrast_ratio(
      base_icon_color, tab_bg_color,
      IsActive() ? kMinimumActiveContrastRatio : kMinimumInactiveContrastRatio);
  const SkColor generated_hovered_color = get_color_for_contrast_ratio(
      base_hovered_color, tab_bg_color, kMinimumHoveredContrastRatio);
  const SkColor generated_pressed_color = get_color_for_contrast_ratio(
      base_pressed_color, tab_bg_color, kMinimumPressedContrastRatio);

  const SkColor generated_hovered_icon_color =
      color_utils::GetColorWithMinimumContrast(base_hovered_pressed_icon_color,
                                               generated_hovered_color);
  const SkColor generated_pressed_icon_color =
      color_utils::GetColorWithMinimumContrast(base_hovered_pressed_icon_color,
                                               generated_pressed_color);
  close_button_->SetIconColors(
      generated_icon_color, generated_hovered_icon_color,
      generated_pressed_icon_color, generated_hovered_color,
      generated_pressed_color);

  if (button_color_ != generated_icon_color) {
    button_color_ = generated_icon_color;
    alert_indicator_button_->OnParentTabButtonColorChanged();
  }
}

Tab::BackgroundCache::BackgroundCache() = default;
Tab::BackgroundCache::~BackgroundCache() = default;
