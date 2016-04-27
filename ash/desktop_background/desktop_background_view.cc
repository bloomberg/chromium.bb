// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_view.h"

#include <limits>

#include "ash/ash_export.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_animations.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"

using wallpaper::WallpaperLayout;
using wallpaper::WALLPAPER_LAYOUT_CENTER;
using wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
using wallpaper::WALLPAPER_LAYOUT_STRETCH;
using wallpaper::WALLPAPER_LAYOUT_TILE;

namespace ash {
namespace {

// A view that controls the child view's layer so that the layer
// always has the same size as the display's original, un-scaled size
// in DIP. The layer then transformed to fit to the virtual screen
// size when laid-out.
// This is to avoid scaling the image at painting time, then scaling
// it back to the screen size in the compositor.
class LayerControlView : public views::View {
 public:
  explicit LayerControlView(views::View* view) {
    AddChildView(view);
    view->SetPaintToLayer(true);
  }

  // Overrides views::View.
  void Layout() override {
    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            GetWidget()->GetNativeView());
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();
    DisplayInfo info = display_manager->GetDisplayInfo(display.id());
    float ui_scale = info.GetEffectiveUIScale();
    gfx::Size rounded_size =
        gfx::ScaleToFlooredSize(display.size(), 1.f / ui_scale);
    DCHECK_EQ(1, child_count());
    views::View* child = child_at(0);
    child->SetBounds(0, 0, rounded_size.width(), rounded_size.height());
    gfx::Transform transform;
    // Apply RTL transform explicitly becacuse Views layer code
    // doesn't handle RTL.  crbug.com/458753.
    transform.Translate(-child->GetMirroredX(), 0);
    transform.Scale(ui_scale, ui_scale);
    child->SetTransform(transform);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LayerControlView);
};

}  // namespace

// This event handler receives events in the pre-target phase and takes care of
// the following:
//   - Disabling overview mode on touch release.
//   - Disabling overview mode on mouse release.
class PreEventDispatchHandler : public ui::EventHandler {
 public:
  PreEventDispatchHandler() {}
  ~PreEventDispatchHandler() override {}

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    CHECK_EQ(ui::EP_PRETARGET, event->phase());
    WindowSelectorController* controller =
        Shell::GetInstance()->window_selector_controller();
    if (event->type() == ui::ET_MOUSE_RELEASED && controller->IsSelecting()) {
      controller->ToggleOverview();
      event->StopPropagation();
    }
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    CHECK_EQ(ui::EP_PRETARGET, event->phase());
    WindowSelectorController* controller =
        Shell::GetInstance()->window_selector_controller();
    if (event->type() == ui::ET_GESTURE_TAP && controller->IsSelecting()) {
      controller->ToggleOverview();
      event->StopPropagation();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(PreEventDispatchHandler);
};

////////////////////////////////////////////////////////////////////////////////
// DesktopBackgroundView, public:

DesktopBackgroundView::DesktopBackgroundView()
    : pre_dispatch_handler_(new PreEventDispatchHandler()) {
  set_context_menu_controller(this);
  AddPreTargetHandler(pre_dispatch_handler_.get());
}

DesktopBackgroundView::~DesktopBackgroundView() {
  RemovePreTargetHandler(pre_dispatch_handler_.get());
}

////////////////////////////////////////////////////////////////////////////////
// DesktopBackgroundView, views::View overrides:

void DesktopBackgroundView::OnPaint(gfx::Canvas* canvas) {
  // Scale the image while maintaining the aspect ratio, cropping as
  // necessary to fill the background. Ideally the image should be larger
  // than the largest display supported, if not we will scale and center it if
  // the layout is WALLPAPER_LAYOUT_CENTER_CROPPED.
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  gfx::ImageSkia wallpaper = controller->GetWallpaper();
  WallpaperLayout wallpaper_layout = controller->GetWallpaperLayout();

  if (wallpaper.isNull()) {
    canvas->FillRect(GetLocalBounds(), SK_ColorBLACK);
    return;
  }

  if (wallpaper_layout == WALLPAPER_LAYOUT_CENTER_CROPPED) {
    // The dimension with the smallest ratio must be cropped, the other one
    // is preserved. Both are set in gfx::Size cropped_size.
    double horizontal_ratio = static_cast<double>(width()) /
        static_cast<double>(wallpaper.width());
    double vertical_ratio = static_cast<double>(height()) /
        static_cast<double>(wallpaper.height());

    gfx::Size cropped_size;
    if (vertical_ratio > horizontal_ratio) {
      cropped_size = gfx::Size(
          gfx::ToFlooredInt(static_cast<double>(width()) / vertical_ratio),
          wallpaper.height());
    } else {
      cropped_size = gfx::Size(
          wallpaper.width(),
          gfx::ToFlooredInt(static_cast<double>(height()) / horizontal_ratio));
    }

    gfx::Rect wallpaper_cropped_rect(
        0, 0, wallpaper.width(), wallpaper.height());
    wallpaper_cropped_rect.ClampToCenteredSize(cropped_size);
    canvas->DrawImageInt(wallpaper,
        wallpaper_cropped_rect.x(), wallpaper_cropped_rect.y(),
        wallpaper_cropped_rect.width(), wallpaper_cropped_rect.height(),
        0, 0, width(), height(),
        true);
  } else if (wallpaper_layout == WALLPAPER_LAYOUT_TILE) {
    canvas->TileImageInt(wallpaper, 0, 0, width(), height());
  } else if (wallpaper_layout == WALLPAPER_LAYOUT_STRETCH) {
    // This is generally not recommended as it may show artifacts.
    canvas->DrawImageInt(wallpaper, 0, 0, wallpaper.width(),
        wallpaper.height(), 0, 0, width(), height(), true);
  } else {
    // Fill with black to make sure that the entire area is opaque.
    canvas->FillRect(GetLocalBounds(), SK_ColorBLACK);
    float image_scale = canvas->image_scale();
    gfx::Rect wallpaper_rect(0, 0, wallpaper.width() / image_scale,
                             wallpaper.height() / image_scale);
    // All other are simply centered, and not scaled (but may be clipped).
    canvas->DrawImageInt(
        wallpaper,
        0, 0, wallpaper.width(), wallpaper.height(),
        (width() - wallpaper_rect.width()) / 2,
        (height() - wallpaper_rect.height()) / 2,
        wallpaper_rect.width(),
        wallpaper_rect.height(),
        true);
  }
}

bool DesktopBackgroundView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void DesktopBackgroundView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  Shell::GetInstance()->ShowContextMenu(point, source_type);
}

views::Widget* CreateDesktopBackground(aura::Window* root_window,
                                       int container_id) {
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  UserWallpaperDelegate* wallpaper_delegate =
      Shell::GetInstance()->user_wallpaper_delegate();

  views::Widget* desktop_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  if (controller->GetWallpaper().isNull())
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = root_window->GetChildById(container_id);
  desktop_widget->Init(params);
  desktop_widget->SetContentsView(
      new LayerControlView(new DesktopBackgroundView()));
  int animation_type = wallpaper_delegate->GetAnimationType();
  ::wm::SetWindowVisibilityAnimationType(desktop_widget->GetNativeView(),
                                         animation_type);

  RootWindowController* root_window_controller =
      GetRootWindowController(root_window);

  // Enable wallpaper transition for the following cases:
  // 1. Initial(OOBE) wallpaper animation.
  // 2. Wallpaper fades in from a non empty background.
  // 3. From an empty background, chrome transit to a logged in user session.
  // 4. From an empty background, guest user logged in.
  if (wallpaper_delegate->ShouldShowInitialAnimation() ||
      root_window_controller->animating_wallpaper_controller() ||
      Shell::GetInstance()->session_state_delegate()->NumberOfLoggedInUsers()) {
    ::wm::SetWindowVisibilityAnimationTransition(
        desktop_widget->GetNativeView(), ::wm::ANIMATE_SHOW);
    int duration_override = wallpaper_delegate->GetAnimationDurationOverride();
    if (duration_override) {
      ::wm::SetWindowVisibilityAnimationDuration(
          desktop_widget->GetNativeView(),
          base::TimeDelta::FromMilliseconds(duration_override));
    }
  } else {
    // Disable animation if transition to login screen from an empty background.
    ::wm::SetWindowVisibilityAnimationTransition(
        desktop_widget->GetNativeView(), ::wm::ANIMATE_NONE);
  }

  desktop_widget->SetBounds(params.parent->bounds());
  return desktop_widget;
}

}  // namespace ash
