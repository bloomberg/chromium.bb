// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_FRAME_NON_CLIENT_FRAME_VIEW_MASH_H_
#define ASH_MUS_FRAME_NON_CLIENT_FRAME_VIEW_MASH_H_

#include "base/macros.h"
#include "services/ui/public/cpp/window_observer.h"
#include "services/ui/public/cpp/window_tree_client_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/paint_cache.h"
#include "ui/views/window/non_client_view.h"

namespace gfx {
class Insets;
}

namespace ui {
class Window;
}

namespace views {
class Widget;
}

namespace ash {
namespace mus {

class FrameCaptionButtonContainerView;

class NonClientFrameViewMash : public views::NonClientFrameView,
                               public ::ui::WindowObserver,
                               public ::ui::WindowTreeClientObserver {
 public:
  // Internal class name.
  static const char kViewClassName[];

  NonClientFrameViewMash(views::Widget* frame, ::ui::Window* window);
  ~NonClientFrameViewMash() override;

  static gfx::Insets GetPreferredClientAreaInsets();
  static int GetMaxTitleBarButtonWidth();

  // Sets the active and inactive frame colors. Note the inactive frame color
  // will have some transparency added when the frame is drawn.
  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // views::View:
  void Layout() override;
  gfx::Size GetPreferredSize() const override;
  const char* GetClassName() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void PaintChildren(const ui::PaintContext& context) override;

  // ui::WindowObserver:
  void OnWindowClientAreaChanged(
      ::ui::Window* window,
      const gfx::Insets& old_client_area,
      const std::vector<gfx::Rect>& old_additional_client_area) override;
  void OnWindowDestroyed(::ui::Window* window) override;
  void OnWindowSharedPropertyChanged(
      ::ui::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;

  // Get the view of the header.
  views::View* GetHeaderView();

 private:
  class OverlayView;

  // Height from top of window to top of client area.
  int NonClientTopBorderHeight() const;

  void RemoveObservers();

  // ui::WindowTreeClientObserver:
  void OnWindowTreeFocusChanged(::ui::Window* gained_focus,
                                ::ui::Window* lost_focus) override;

  // Not owned.
  views::Widget* frame_;

  ::ui::Window* window_;
  ui::PaintCache paint_cache_;

  // View which contains the title and window controls.
  class HeaderView;
  HeaderView* header_view_;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameViewMash);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_FRAME_NON_CLIENT_FRAME_VIEW_MASH_H_
