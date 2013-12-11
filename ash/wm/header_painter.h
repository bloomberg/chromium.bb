// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_HEADER_PAINTER_H_
#define ASH_WM_HEADER_PAINTER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"  // OVERRIDE
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}
namespace gfx {
class Canvas;
class Font;
class ImageSkia;
class Point;
class Size;
class SlideAnimation;
}
namespace views {
class View;
class Widget;
}

namespace ash {
class FrameCaptionButtonContainerView;

// Helper class for painting the window header.
class ASH_EXPORT HeaderPainter : public aura::WindowObserver,
                                 public gfx::AnimationDelegate {
 public:
  // Opacity values for the window header in various states, from 0 to 255.
  static int kActiveWindowOpacity;
  static int kInactiveWindowOpacity;
  static int kSoloWindowOpacity;

  enum HeaderMode {
    ACTIVE,
    INACTIVE
  };

  HeaderPainter();
  virtual ~HeaderPainter();

  // None of the parameters are owned.
  void Init(views::Widget* frame,
            views::View* header_view,
            views::View* window_icon,
            FrameCaptionButtonContainerView* caption_button_container);

  // Returns the bounds of the client view for a window with |header_height|
  // and |window_bounds|. The return value and |window_bounds| are in the
  // views::NonClientView's coordinates.
  static gfx::Rect GetBoundsForClientView(int header_height,
                                          const gfx::Rect& window_bounds);

  // Returns the bounds of the window given |header_height| and |client_bounds|.
  // The return value and |client_bounds| are in the views::NonClientView's
  // coordinates.
  static gfx::Rect GetWindowBoundsForClientBounds(
      int header_height,
      const gfx::Rect& client_bounds);

  // Determines the window HT* code at |point|. Returns HTNOWHERE if |point| is
  // not within the top |header_height_| of |header_view_|. |point| is in the
  // coordinates of |header_view_|'s widget. The client view must be hittested
  // before calling this method because a browser's tabs are in the top
  // |header_height_| of |header_view_|.
  int NonClientHitTest(const gfx::Point& point) const;

  // Returns the header's minimum width.
  int GetMinimumHeaderWidth() const;

  // Returns the inset from the right edge.
  int GetRightInset() const;

  // Returns the amount that the theme background should be inset.
  int GetThemeBackgroundXInset() const;

  // Paints the header.
  // |theme_frame_overlay_id| is 0 if no overlay image should be used.
  void PaintHeader(gfx::Canvas* canvas,
                   HeaderMode header_mode,
                   int theme_frame_id,
                   int theme_frame_overlay_id);

  // Paints the header/content separator line. Exists as a separate function
  // because some windows with complex headers (e.g. browsers with tab strips)
  // need to draw their own line.
  void PaintHeaderContentSeparator(gfx::Canvas* canvas);

  // Returns size of the header/content separator line in pixels.
  int HeaderContentSeparatorSize() const;

  // Paint the title bar, primarily the title string.
  void PaintTitleBar(gfx::Canvas* canvas, const gfx::Font& title_font);

  // Performs layout for the header based on whether we want the shorter
  // appearance. |shorter_layout| is typically used for maximized windows, but
  // not always.
  void LayoutHeader(bool shorter_layout);

  // Sets the height of the header. The height of the header affects painting,
  // and non client hit tests. It does not affect layout.
  void set_header_height(int header_height) {
    header_height_ = header_height;
  }

  // Returns the header height.
  int header_height() const {
    return header_height_;
  }

  // Schedule a re-paint of the entire title.
  void SchedulePaintForTitle(const gfx::Font& title_font);

  // Called when the browser theme changes.
  void OnThemeChanged();

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // Overridden from gfx::AnimationDelegate
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(HeaderPainterTest, GetHeaderOpacity);
  FRIEND_TEST_ALL_PREFIXES(HeaderPainterTest, TitleIconAlignment);

  // Returns the header bounds in the coordinates of |header_view_|. The header
  // is assumed to be positioned at the top left corner of |header_view_| and to
  // have the same width as |header_view_|.
  gfx::Rect GetHeaderLocalBounds() const;

  // Returns the offset between window left edge and title string.
  int GetTitleOffsetX() const;

  // Returns the vertical center of the caption button container in window
  // coordinates.
  int GetCaptionButtonContainerCenterY() const;

  // Returns the opacity value used to paint the header.
  // |theme_frame_overlay_id| is 0 if no overlay image is used.
  int GetHeaderOpacity(HeaderMode header_mode,
                       int theme_frame_id,
                       int theme_frame_overlay_id) const;

  // Returns the radius of the header's top corners.
  int GetHeaderCornerRadius() const;

  // Schedules a paint for the header. Used when transitioning from no header to
  // a header (or other way around).
  void SchedulePaintForHeader();

  // Get the bounds for the title. The provided |title_font| is used to
  // determine the correct dimensions.
  gfx::Rect GetTitleBounds(const gfx::Font& title_font);

  // Not owned
  views::Widget* frame_;
  views::View* header_view_;
  views::View* window_icon_;  // May be NULL.
  FrameCaptionButtonContainerView* caption_button_container_;
  aura::Window* window_;

  // The height of the header.
  int header_height_;

  // Window frame header/caption parts.
  const gfx::ImageSkia* top_left_corner_;
  const gfx::ImageSkia* top_edge_;
  const gfx::ImageSkia* top_right_corner_;
  const gfx::ImageSkia* header_left_edge_;
  const gfx::ImageSkia* header_right_edge_;

  // Image ids and opacity last used for painting header.
  int previous_theme_frame_id_;
  int previous_theme_frame_overlay_id_;
  int previous_opacity_;

  // Image ids and opacity we are crossfading from.
  int crossfade_theme_frame_id_;
  int crossfade_theme_frame_overlay_id_;
  int crossfade_opacity_;

  scoped_ptr<gfx::SlideAnimation> crossfade_animation_;

  DISALLOW_COPY_AND_ASSIGN(HeaderPainter);
};

}  // namespace ash

#endif  // ASH_WM_HEADER_PAINTER_H_
