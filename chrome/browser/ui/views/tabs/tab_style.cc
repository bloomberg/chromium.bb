// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_style.h"

#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns whether we should extend the hit test region for Fitts' Law.
bool ShouldExtendHitTest(const Tab* tab) {
  const views::Widget* widget = tab->GetWidget();
  return widget->IsMaximized() || widget->IsFullscreen();
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

class GM2TabStyle : public TabStyle {
 public:
  gfx::Path GetPath(
      const Tab* tab,
      PathType path_type,
      float scale,
      bool force_active = false,
      RenderUnits render_units = RenderUnits::kPixels) const override {
    const float stroke_thickness = tab->GetStrokeThickness(force_active);

    // We'll do the entire path calculation in aligned pixels.
    // TODO(dfried): determine if we actually want to use |stroke_thickness| as
    // the inset in this case.
    gfx::RectF aligned_bounds =
        ScaleAndAlignBounds(tab->bounds(), scale, stroke_thickness);

    if (path_type == PathType::kInteriorClip) {
      // When there is a separator, animate the clip to account for it, in sync
      // with the separator's fading.
      // TODO(pkasting): Consider crossfading the favicon instead of animating
      // the clip, especially if other children get crossfaded.
      const auto opacities = tab->GetSeparatorOpacities(true);
      constexpr float kChildClipPadding = 2.5f;
      aligned_bounds.Inset(
          gfx::InsetsF(0.0f, kChildClipPadding + opacities.left, 0.0f,
                       kChildClipPadding + opacities.right));
    }

    // Calculate the corner radii. Note that corner radius is based on original
    // tab width (in DIP), not our new, scaled-and-aligned bounds.
    const float radius = GetTopCornerRadiusForWidth(tab->width()) * scale;
    float top_radius = radius;
    float bottom_radius = radius;

    // Compute |extension| as the width outside the separators.  This is a
    // fixed value equal to the normal corner radius.
    const float extension = Tab::GetCornerRadius() * scale;

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
        (path_type == PathType::kHitTest) && ShouldExtendHitTest(tab);

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
    if (path_type != PathType::kBorder) {
      path.close();
    }

    // Convert path to be relative to the tab origin.
    gfx::PointF origin(tab->origin());
    origin.Scale(scale);
    path.offset(-origin.x(), -origin.y());

    // Possibly convert back to DIPs.
    if (render_units == RenderUnits::kDips && scale != 1.0f)
      path.transform(SkMatrix::MakeScale(1.f / scale));

    return path;
  }

  SeparatorBounds GetSeparatorBounds(const Tab* tab,
                                     float scale) const override {
    const gfx::RectF aligned_bounds =
        ScaleAndAlignBounds(tab->bounds(), scale, tab->GetStrokeThickness());
    const int corner_radius = Tab::GetCornerRadius() * scale;
    const float separator_height = Tab::GetTabSeparatorHeight() * scale;
    const float separator_thickness = Tab::kSeparatorThickness * scale;

    SeparatorBounds separator_bounds;

    separator_bounds.leading = gfx::RectF(
        aligned_bounds.x() + corner_radius,
        aligned_bounds.y() + (aligned_bounds.height() - separator_height) / 2,
        separator_thickness, separator_height);

    separator_bounds.trailing = separator_bounds.leading;
    separator_bounds.trailing.set_x(aligned_bounds.right() -
                                    (corner_radius + separator_thickness));

    gfx::PointF origin(tab->bounds().origin());
    origin.Scale(scale);
    separator_bounds.leading.Offset(-origin.x(), -origin.y());
    separator_bounds.trailing.Offset(-origin.x(), -origin.y());

    return separator_bounds;
  }
};

}  // namespace

// static
const TabStyle* TabStyle::GetInstance() {
  static base::NoDestructor<GM2TabStyle> instance;

  return instance.get();
}
