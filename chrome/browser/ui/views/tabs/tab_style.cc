// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_style.h"

#include <algorithm>
#include <utility>

#include "base/numerics/ranges.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/widget/widget.h"

namespace {

// Thickness in DIPs of the separator painted on the left and right edges of
// the tab.
constexpr int kSeparatorThickness = 1;

// Returns the radius of the outer corners of the tab shape.
int GetCornerRadius() {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_HIGH);
}

// Returns how far from the leading and trailing edges of a tab the contents
// should actually be laid out.
int GetContentsHorizontalInsetSize() {
  return GetCornerRadius() * 2;
}

// Returns the height of the separator between tabs.
int GetSeparatorHeight() {
  constexpr int kTabSeparatorHeight = 20;
  constexpr int kTabSeparatorTouchHeight = 24;
  return ui::MaterialDesignController::IsTouchOptimizedUiEnabled()
             ? kTabSeparatorTouchHeight
             : kTabSeparatorHeight;
}

class GM2TabStyle : public TabStyle {
 public:
  explicit GM2TabStyle(const Tab* tab) : tab_(tab) {}

  gfx::Path GetPath(
      PathType path_type,
      float scale,
      bool force_active = false,
      RenderUnits render_units = RenderUnits::kPixels) const override {
    const int stroke_thickness = GetStrokeThickness(force_active);

    // We'll do the entire path calculation in aligned pixels.
    // TODO(dfried): determine if we actually want to use |stroke_thickness| as
    // the inset in this case.
    gfx::RectF aligned_bounds =
        ScaleAndAlignBounds(tab_->bounds(), scale, stroke_thickness);

    if (path_type == PathType::kInteriorClip) {
      // When there is a separator, animate the clip to account for it, in sync
      // with the separator's fading.
      // TODO(pkasting): Consider crossfading the favicon instead of animating
      // the clip, especially if other children get crossfaded.
      const auto opacities = GetSeparatorOpacities(true);
      constexpr float kChildClipPadding = 2.5f;
      aligned_bounds.Inset(
          gfx::InsetsF(0.0f, kChildClipPadding + opacities.left, 0.0f,
                       kChildClipPadding + opacities.right));
    }

    // Calculate the corner radii. Note that corner radius is based on original
    // tab width (in DIP), not our new, scaled-and-aligned bounds.
    const float radius = GetTopCornerRadiusForWidth(tab_->width()) * scale;
    float top_radius = radius;
    float bottom_radius = radius;

    // Compute |extension| as the width outside the separators.  This is a
    // fixed value equal to the normal corner radius.
    const float extension = GetCornerRadius() * scale;

    // Calculate the bounds of the actual path.
    const float left = aligned_bounds.x();
    const float right = aligned_bounds.right();
    float tab_top = aligned_bounds.y();
    float tab_left = left + extension;
    float tab_right = right - extension;

    // Overlap the toolbar below us so that gaps don't occur when rendering at
    // non-integral display scale factors.
    const float extended_bottom = aligned_bounds.bottom();
    const float bottom_extension =
        GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) * scale;
    float tab_bottom = extended_bottom - bottom_extension;

    // Path-specific adjustments:
    const float stroke_adjustment = stroke_thickness * scale;
    if (path_type == PathType::kInteriorClip) {
      // Inside of the border runs |stroke_thickness| inside the outer edge.
      tab_left += stroke_adjustment;
      tab_right -= stroke_adjustment;
      tab_top += stroke_adjustment;
      top_radius -= stroke_adjustment;
    } else if (path_type == PathType::kFill || path_type == PathType::kBorder) {
      tab_left += 0.5f * stroke_adjustment;
      tab_right -= 0.5f * stroke_adjustment;
      tab_top += 0.5f * stroke_adjustment;
      top_radius -= 0.5f * stroke_adjustment;
      tab_bottom -= 0.5f * stroke_adjustment;
      bottom_radius -= 0.5f * stroke_adjustment;
    } else if (path_type == PathType::kHitTest ||
               path_type == PathType::kExteriorClip) {
      // Outside border needs to draw its bottom line a stroke width above the
      // bottom of the tab, to line up with the stroke that runs across the rest
      // of the bottom of the tab bar (when strokes are enabled).
      tab_bottom -= stroke_adjustment;
      bottom_radius -= stroke_adjustment;
    }
    const bool extend_to_top =
        (path_type == PathType::kHitTest) && ShouldExtendHitTest();

    // When the radius shrinks, it leaves a gap between the bottom corners and
    // the edge of the tab. Make sure we account for this - and for any
    // adjustment we may have made to the location of the tab!
    const float corner_gap = (right - tab_right) - bottom_radius;

    gfx::Path path;

    // We will go clockwise from the lower left. We start in the overlap
    // region, preventing a gap between toolbar and tabstrip.
    // TODO(dfried): verify that the we actually want to start the stroke for
    // the exterior path outside the region; we might end up rendering an
    // extraneous descending pixel on displays with odd scaling and nonzero
    // stroke width.

    // Start with the left side of the shape.

    // Draw everything left of the bottom-left corner of the tab.
    //   ╭─────────╮
    //   │ Content │
    // ┏━╯         ╰─┐
    path.moveTo(left, extended_bottom);
    path.lineTo(left, tab_bottom);
    path.lineTo(left + corner_gap, tab_bottom);

    // Draw the bottom-left arc.
    //   ╭─────────╮
    //   │ Content │
    // ┌─╝         ╰─┐
    path.arcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
               SkPath::kCCW_Direction, tab_left, tab_bottom - bottom_radius);

    // Draw the ascender and top arc, if present.
    if (extend_to_top) {
      //   ┎─────────╮
      //   ┃ Content │
      // ┌─╯         ╰─┐
      path.lineTo(tab_left, tab_top);
    } else {
      //   ╔─────────╮
      //   ┃ Content │
      // ┌─╯         ╰─┐
      path.lineTo(tab_left, tab_top + top_radius);
      path.arcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                 SkPath::kCW_Direction, tab_left + top_radius, tab_top);
    }

    // Draw the top crossbar and top-right curve, if present.
    if (extend_to_top) {
      //   ┌━━━━━━━━━┑
      //   │ Content │
      // ┌─╯         ╰─┐
      path.lineTo(tab_right, tab_top);

    } else {
      //   ╭━━━━━━━━━╗
      //   │ Content │
      // ┌─╯         ╰─┐
      path.lineTo(tab_right - top_radius, tab_top);
      path.arcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                 SkPath::kCW_Direction, tab_right, tab_top + top_radius);
    }

    // Draw the descender and bottom-right arc.
    //   ╭─────────╮
    //   │ Content ┃
    // ┌─╯         ╚─┐
    path.lineTo(tab_right, tab_bottom - bottom_radius);
    path.arcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
               SkPath::kCCW_Direction, right - corner_gap, tab_bottom);

    // Draw everything right of the bottom-right corner of the tab.
    //   ╭─────────╮
    //   │ Content │
    // ┌─╯         ╰━┓
    path.lineTo(right, tab_bottom);
    path.lineTo(right, extended_bottom);

    // Finish the path.
    if (path_type != PathType::kBorder)
      path.close();

    // Convert path to be relative to the tab origin.
    gfx::PointF origin(tab_->origin());
    origin.Scale(scale);
    path.offset(-origin.x(), -origin.y());

    // Possibly convert back to DIPs.
    if (render_units == RenderUnits::kDips && scale != 1.0f)
      path.transform(SkMatrix::MakeScale(1.f / scale));

    return path;
  }

  SeparatorBounds GetSeparatorBounds(float scale) const override {
    const gfx::RectF aligned_bounds =
        ScaleAndAlignBounds(tab_->bounds(), scale, GetStrokeThickness());
    const int corner_radius = GetCornerRadius() * scale;
    gfx::SizeF separator_size(GetSeparatorSize());
    separator_size.Scale(scale);

    SeparatorBounds separator_bounds;

    separator_bounds.leading =
        gfx::RectF(aligned_bounds.x() + corner_radius,
                   aligned_bounds.y() +
                       (aligned_bounds.height() - separator_size.height()) / 2,
                   separator_size.width(), separator_size.height());

    separator_bounds.trailing = separator_bounds.leading;
    separator_bounds.trailing.set_x(aligned_bounds.right() -
                                    (corner_radius + separator_size.width()));

    gfx::PointF origin(tab_->bounds().origin());
    origin.Scale(scale);
    separator_bounds.leading.Offset(-origin.x(), -origin.y());
    separator_bounds.trailing.Offset(-origin.x(), -origin.y());

    return separator_bounds;
  }

  gfx::Insets GetContentsInsets() const override {
    const int stroke_thickness = GetStrokeThickness();
    const int horizontal_inset = GetContentsHorizontalInsetSize();
    return gfx::Insets(
        stroke_thickness, horizontal_inset,
        stroke_thickness + GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP),
        horizontal_inset);
  }

  int GetStrokeThickness(bool should_paint_as_active = false) const override {
    return (tab_->IsActive() || should_paint_as_active)
               ? tab_->controller()->GetStrokeThickness()
               : 0;
  }

  SeparatorOpacities GetSeparatorOpacities(bool for_layout) const override {
    // Something should visually separate tabs from each other and any adjacent
    // new tab button.  Normally, active and hovered tabs draw distinct shapes
    // (via different background colors) and thus need no separators, while
    // background tabs need separators between them.  In single-tab mode, the
    // active tab has no visible shape and thus needs separators on any side
    // with an adjacent new tab button.  (The other sides will be faded out
    // below.)
    float leading_opacity, trailing_opacity;
    if (tab_->controller()->SingleTabMode()) {
      leading_opacity = trailing_opacity = 1.f;
    } else if (tab_->IsActive()) {
      leading_opacity = trailing_opacity = 0;
    } else {
      // Fade out the trailing separator while this tab or the subsequent tab is
      // hovered.  If the subsequent tab is active, don't consider its hover
      // animation value, lest the trailing separator on this tab disappear
      // while the subsequent tab is being dragged.
      const float hover_value = tab_->hover_controller()->GetAnimationValue();
      const Tab* subsequent_tab = tab_->controller()->GetAdjacentTab(tab_, 1);
      const float subsequent_hover =
          !for_layout && subsequent_tab && !subsequent_tab->IsActive()
              ? float{subsequent_tab->hover_controller()->GetAnimationValue()}
              : 0;
      trailing_opacity = 1.f - std::max(hover_value, subsequent_hover);

      // The leading separator need not consider the previous tab's hover value,
      // since if there is a previous tab that's hovered and not being dragged,
      // it will draw atop this tab.
      leading_opacity = 1.f - hover_value;

      const Tab* previous_tab = tab_->controller()->GetAdjacentTab(tab_, -1);
      if (tab_->IsSelected()) {
        // Since this tab is selected, its shape will be visible against
        // adjacent unselected tabs, so remove the separator in those cases.
        if (previous_tab && !previous_tab->IsSelected())
          leading_opacity = 0;
        if (subsequent_tab && !subsequent_tab->IsSelected())
          trailing_opacity = 0;
      } else if (tab_->controller()->HasVisibleBackgroundTabShapes()) {
        // Since this tab is unselected, adjacent selected tabs will normally
        // paint atop it, covering the separator.  But if the user drags those
        // selected tabs away, the exposed region looks like the window frame;
        // and since background tab shapes are visible, there should be no
        // separator.
        // TODO(pkasting): https://crbug.com/876599  When a tab is animating
        // into this gap, we should adjust its separator opacities as well.
        if (previous_tab && previous_tab->IsSelected())
          leading_opacity = 0;
        if (subsequent_tab && subsequent_tab->IsSelected())
          trailing_opacity = 0;
      }
    }

    // For the first or last tab in the strip, fade the leading or trailing
    // separator based on the NTB position and how close to the target bounds
    // this tab is.  In the steady state, this hides separators on the opposite
    // end of the strip from the NTB; it fades out the separators as tabs
    // animate into these positions, after they pass by the other tabs; and it
    // snaps the separators to full visibility immediately when animating away
    // from these positions, which seems desirable.
    const NewTabButtonPosition ntb_position =
        tab_->controller()->GetNewTabButtonPosition();
    const gfx::Rect target_bounds =
        tab_->controller()->GetTabAnimationTargetBounds(tab_);
    const int tab_width = std::max(tab_->width(), target_bounds.width());
    const float target_opacity =
        float{std::min(std::abs(tab_->x() - target_bounds.x()), tab_width)} /
        tab_width;
    // If the tab shapes are visible, never draw end separators.
    const bool always_hide_separators_on_ends =
        tab_->controller()->HasVisibleBackgroundTabShapes();
    if (tab_->controller()->IsFirstVisibleTab(tab_) &&
        (ntb_position != LEADING || always_hide_separators_on_ends))
      leading_opacity = target_opacity;
    if (tab_->controller()->IsLastVisibleTab(tab_) &&
        (ntb_position != AFTER_TABS || always_hide_separators_on_ends))
      trailing_opacity = target_opacity;

    // Return the opacities in physical order, rather than logical.
    if (base::i18n::IsRTL())
      std::swap(leading_opacity, trailing_opacity);
    return {leading_opacity, trailing_opacity};
  }

 private:
  const Tab* const tab_;

  // Returns whether we shoould extend the hit test region for Fitts' Law.
  bool ShouldExtendHitTest() const {
    const views::Widget* widget = tab_->GetWidget();
    return widget->IsMaximized() || widget->IsFullscreen();
  }

  // Given a tab of width |width|, returns the radius to use for the corners.
  static float GetTopCornerRadiusForWidth(int width) {
    // Get the width of the top of the tab by subtracting the width of the outer
    // corners.
    const int ideal_radius = GetCornerRadius();
    const int top_width = width - ideal_radius * 2;

    // To maintain a round-rect appearance, ensure at least one third of the top
    // of the tab is flat.
    const float radius = top_width / 3.f;
    return base::ClampToRange<float>(radius, 0, ideal_radius);
  }

  // Scales |bounds| by scale and aligns so that adjacent tabs meet up exactly
  // during painting.
  static gfx::RectF ScaleAndAlignBounds(const gfx::Rect& bounds,
                                        float scale,
                                        int stroke_thickness) {
    // Convert to layout bounds.  We must inset the width such that the right
    // edge of one tab's layout bounds is the same as the left edge of the next
    // tab's; this way the two tabs' separators will be drawn at the same
    // coordinate.
    gfx::RectF aligned_bounds(bounds);
    const int corner_radius = GetCornerRadius();
    // Note: This intentionally doesn't subtract TABSTRIP_TOOLBAR_OVERLAP from
    // the bottom inset, because we want to pixel-align the bottom of the
    // stroke, not the bottom of the overlap.
    gfx::InsetsF layout_insets(stroke_thickness, corner_radius,
                               stroke_thickness,
                               corner_radius + kSeparatorThickness);
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

    // Convert back to full bounds.  It's OK that the outer corners of the
    // curves around the separator may not be snapped to the pixel grid as a
    // result.
    aligned_bounds.Inset(-layout_insets.Scale(scale));
    return aligned_bounds;
  }
};

}  // namespace

// TabStyle --------------------------------------------------------------------

TabStyle::~TabStyle() = default;

// static
TabStyle* TabStyle::CreateForTab(const Tab* tab) {
  return new GM2TabStyle(tab);
}

// static
int TabStyle::GetMinimumActiveWidth() {
  return TabCloseButton::GetWidth() + GetContentsHorizontalInsetSize() * 2;
}

// static
int TabStyle::GetMinimumInactiveWidth() {
  // Allow tabs to shrink until they appear to be 16 DIP wide excluding
  // outer corners.
  constexpr int kInteriorWidth = 16;
  // The overlap contains the trailing separator that is part of the
  // interior width; avoid double-counting it.
  return kInteriorWidth - kSeparatorThickness + GetTabOverlap();
}

// static
int TabStyle::GetStandardWidth() {
  // The standard tab width is 240 DIP including both separators.
  constexpr int kTabWidth = 240;
  // The overlap includes one separator, so subtract it here.
  return kTabWidth + GetTabOverlap() - kSeparatorThickness;
}

// static
int TabStyle::GetPinnedWidth() {
  constexpr int kTabPinnedContentWidth = 23;
  return kTabPinnedContentWidth + GetContentsHorizontalInsetSize() * 2;
}

// static
int TabStyle::GetTabOverlap() {
  return GetCornerRadius() * 2 + kSeparatorThickness;
}

// static
int TabStyle::GetDragHandleExtension(int height) {
  return (height - GetSeparatorHeight()) / 2 - 1;
}

// static
gfx::Insets TabStyle::GetTabInternalPadding() {
  return gfx::Insets(0, GetCornerRadius());
}

// static
gfx::Size TabStyle::GetSeparatorSize() {
  return gfx::Size(kSeparatorThickness, GetSeparatorHeight());
}
