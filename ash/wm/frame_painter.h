// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FRAME_PAINTER_H_
#define ASH_WM_FRAME_PAINTER_H_

#include <set>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"  // OVERRIDE
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
class Window;
}
namespace gfx {
class Canvas;
class Font;
class ImageSkia;
class Point;
class Size;
}
namespace ui {
class SlideAnimation;
}
namespace views {
class ImageButton;
class NonClientFrameView;
class ToggleImageButton;
class View;
class Widget;
}

namespace ash {

// Helper class for painting window frames.  Exists to share code between
// various implementations of views::NonClientFrameView.  Canonical source of
// layout constants for Ash window frames.
class ASH_EXPORT FramePainter : public aura::WindowObserver,
                                public ui::AnimationDelegate {
 public:
  // Opacity values for the window header in various states, from 0 to 255.
  static int kActiveWindowOpacity;
  static int kInactiveWindowOpacity;
  static int kSoloWindowOpacity;

  enum HeaderMode {
    ACTIVE,
    INACTIVE
  };

  // What happens when the |size_button_| is pressed.
  enum SizeButtonBehavior {
    SIZE_BUTTON_MINIMIZES,
    SIZE_BUTTON_MAXIMIZES
  };

  FramePainter();
  virtual ~FramePainter();

  // |frame| and buttons are used for layout and are not owned.
  void Init(views::Widget* frame,
            views::View* window_icon,
            views::ImageButton* size_button,
            views::ImageButton* close_button,
            SizeButtonBehavior behavior);

  // Updates the solo-window transparent header appearance for all windows
  // using frame painters across all root windows.
  static void UpdateSoloWindowHeader(aura::RootWindow* root_window);

  // Helpers for views::NonClientFrameView implementations.
  gfx::Rect GetBoundsForClientView(int top_height,
                                   const gfx::Rect& window_bounds) const;
  gfx::Rect GetWindowBoundsForClientBounds(
      int top_height,
      const gfx::Rect& client_bounds) const;
  int NonClientHitTest(views::NonClientFrameView* view,
                       const gfx::Point& point);
  gfx::Size GetMinimumSize(views::NonClientFrameView* view);
  gfx::Size GetMaximumSize(views::NonClientFrameView* view);

  // Returns the inset from the right edge.
  int GetRightInset() const;

  // Returns the amount that the theme background should be inset.
  int GetThemeBackgroundXInset() const;

  // Paints the frame header.
  void PaintHeader(views::NonClientFrameView* view,
                   gfx::Canvas* canvas,
                   HeaderMode header_mode,
                   int theme_frame_id,
                   const gfx::ImageSkia* theme_frame_overlay);

  // Paints the header/content separator line.  Exists as a separate function
  // because some windows with complex headers (e.g. browsers with tab strips)
  // need to draw their own line.
  void PaintHeaderContentSeparator(views::NonClientFrameView* view,
                                   gfx::Canvas* canvas);

  // Returns size of the header/content separator line in pixels.
  int HeaderContentSeparatorSize() const;

  // Paint the title bar, primarily the title string.
  void PaintTitleBar(views::NonClientFrameView* view,
                     gfx::Canvas* canvas,
                     const gfx::Font& title_font);

  // Performs layout for the header based on whether we want the shorter
  // appearance. |shorter_layout| is typically used for maximized windows, but
  // not always.
  void LayoutHeader(views::NonClientFrameView* view, bool shorter_layout);

  // Schedule a re-paint of the entire title.
  void SchedulePaintForTitle(views::NonClientFrameView* view,
                             const gfx::Font& title_font);

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowAddedToRootWindow(aura::Window* window) OVERRIDE;
  virtual void OnWindowRemovingFromRootWindow(aura::Window* window) OVERRIDE;

  // Overridden from ui::AnimationDelegate
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(FramePainterTest, Basics);
  FRIEND_TEST_ALL_PREFIXES(FramePainterTest, CreateAndDeleteSingleWindow);
  FRIEND_TEST_ALL_PREFIXES(FramePainterTest, UseSoloWindowHeader);
  FRIEND_TEST_ALL_PREFIXES(FramePainterTest, UseSoloWindowHeaderMultiDisplay);
  FRIEND_TEST_ALL_PREFIXES(FramePainterTest, GetHeaderOpacity);

  // Sets the images for a button based on IDs from the |frame_| theme provider.
  void SetButtonImages(views::ImageButton* button,
                       int normal_image_id,
                       int hot_image_id,
                       int pushed_image_id);

  // Sets the toggled-state button images for a button based on IDs from the
  // |frame_| theme provider.
  void SetToggledButtonImages(views::ToggleImageButton* button,
                              int normal_image_id,
                              int hot_image_id,
                              int pushed_image_id);

  // Returns the offset between window left edge and title string.
  int GetTitleOffsetX() const;

  // Returns the opacity value used to paint the header.
  int GetHeaderOpacity(HeaderMode header_mode,
                       int theme_frame_id,
                       const gfx::ImageSkia* theme_frame_overlay);

  // Adjust frame operations for left / right maximized modes.
  int AdjustFrameHitCodeForMaximizedModes(int hit_code);

  // Returns true if |window_| is exactly one visible, normal-type window in
  // |window_->GetRootWindow()|, in which case we should paint a transparent
  // window header.
  bool UseSoloWindowHeader();

  // Returns the frame painter for the solo window in |root_window|. Returns
  // NULL in case there is no such window, for example more than two windows or
  // there's a fullscreen window.  It ignores |ignorable_window| to check the
  // solo-ness of the window.  Pass NULL for |ignorable_window| to consider
  // all windows.
  static FramePainter* GetSoloPainterInRoot(aura::RootWindow* root_window,
                                            aura::Window* ignorable_window);

  // Updates the current solo window frame painter for |root_window| while
  // ignoring |ignorable_window|. If the solo window frame painter changed it
  // schedules paints as necessary.
  static void UpdateSoloWindowInRoot(aura::RootWindow* root_window,
                                     aura::Window* ignorable_window);

  // Convenience method to call UpdateSoloWindowInRoot() with the current
  // window's root window.
  void UpdateSoloWindowFramePainter(aura::Window* ignorable_window);

  // Schedules a paint for the header. Used when transitioning from no header to
  // a header (or other way around).
  void SchedulePaintForHeader();

  // Get the bounds for the title. The provided |view| and |title_font| are
  // used to determine the correct dimensions.
  gfx::Rect GetTitleBounds(views::NonClientFrameView* view,
                           const gfx::Font& title_font);

  static std::set<FramePainter*>* instances_;

  // Not owned
  views::Widget* frame_;
  views::View* window_icon_;  // May be NULL.
  views::ImageButton* size_button_;
  views::ImageButton* close_button_;
  aura::Window* window_;

  // Window frame header/caption parts.
  const gfx::ImageSkia* button_separator_;
  const gfx::ImageSkia* top_left_corner_;
  const gfx::ImageSkia* top_edge_;
  const gfx::ImageSkia* top_right_corner_;
  const gfx::ImageSkia* header_left_edge_;
  const gfx::ImageSkia* header_right_edge_;

  // Image id and opacity last used for painting header.
  int previous_theme_frame_id_;
  int previous_opacity_;

  // Image id and opacity we are crossfading from.
  int crossfade_theme_frame_id_;
  int crossfade_opacity_;

  gfx::Rect header_frame_bounds_;
  scoped_ptr<ui::SlideAnimation> crossfade_animation_;

  SizeButtonBehavior size_button_behavior_;

  DISALLOW_COPY_AND_ASSIGN(FramePainter);
};

}  // namespace ash

#endif  // ASH_WM_FRAME_PAINTER_H_
