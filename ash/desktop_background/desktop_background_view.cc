// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_view.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/display/display_info.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// A view that controls the child view's layer so that the layer always has the
// same size as the display's original, un-scaled size in DIP. The layer is then
// transformed to fit to the virtual screen size when laid-out. This is to avoid
// scaling the image at painting time, then scaling it back to the screen size
// in the compositor.
class LayerControlView : public views::View {
 public:
  explicit LayerControlView(views::View* view) {
    AddChildView(view);
    view->SetPaintToLayer(true);
  }

  // Overrides views::View.
  void Layout() override {
    WmWindow* window = WmLookup::Get()->GetWindowForWidget(GetWidget());
    // Keep |this| at the bottom since there may be other windows on top of the
    // background view such as an overview mode shield.
    window->GetParent()->StackChildAtBottom(window);
    display::Display display = window->GetDisplayNearestWindow();
    DisplayInfo info = WmShell::Get()->GetDisplayInfo(display.id());
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
        WmShell::Get()->window_selector_controller();
    if (event->type() == ui::ET_MOUSE_RELEASED && controller->IsSelecting()) {
      controller->ToggleOverview();
      event->StopPropagation();
    }
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    CHECK_EQ(ui::EP_PRETARGET, event->phase());
    WindowSelectorController* controller =
        WmShell::Get()->window_selector_controller();
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
  // the layout is wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED.
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  gfx::ImageSkia wallpaper = controller->GetWallpaper();
  wallpaper::WallpaperLayout layout = controller->GetWallpaperLayout();

  // Wallpapers with png format could be partially transparent.
  // Fill the canvas with black background to make it opaque
  // before painting wallpaper
  canvas->FillRect(GetLocalBounds(), SK_ColorBLACK);

  if (wallpaper.isNull())
    return;

  if (layout == wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED) {
    // The dimension with the smallest ratio must be cropped, the other one
    // is preserved. Both are set in gfx::Size cropped_size.
    double horizontal_ratio =
        static_cast<double>(width()) / static_cast<double>(wallpaper.width());
    double vertical_ratio =
        static_cast<double>(height()) / static_cast<double>(wallpaper.height());

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

    gfx::Rect wallpaper_cropped_rect(0, 0, wallpaper.width(),
                                     wallpaper.height());
    wallpaper_cropped_rect.ClampToCenteredSize(cropped_size);
    canvas->DrawImageInt(
        wallpaper, wallpaper_cropped_rect.x(), wallpaper_cropped_rect.y(),
        wallpaper_cropped_rect.width(), wallpaper_cropped_rect.height(), 0, 0,
        width(), height(), true);
  } else if (layout == wallpaper::WALLPAPER_LAYOUT_TILE) {
    canvas->TileImageInt(wallpaper, 0, 0, width(), height());
  } else if (layout == wallpaper::WALLPAPER_LAYOUT_STRETCH) {
    // This is generally not recommended as it may show artifacts.
    canvas->DrawImageInt(wallpaper, 0, 0, wallpaper.width(), wallpaper.height(),
                         0, 0, width(), height(), true);
  } else {
    float image_scale = canvas->image_scale();
    gfx::Rect wallpaper_rect(0, 0, wallpaper.width() / image_scale,
                             wallpaper.height() / image_scale);
    // All other are simply centered, and not scaled (but may be clipped).
    canvas->DrawImageInt(wallpaper, 0, 0, wallpaper.width(), wallpaper.height(),
                         (width() - wallpaper_rect.width()) / 2,
                         (height() - wallpaper_rect.height()) / 2,
                         wallpaper_rect.width(), wallpaper_rect.height(), true);
  }
}

bool DesktopBackgroundView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void DesktopBackgroundView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  WmShell::Get()->ShowContextMenu(point, source_type);
}

views::Widget* CreateDesktopBackground(WmWindow* root_window,
                                       int container_id) {
  aura::Window* aura_root_window = WmWindowAura::GetAuraWindow(root_window);
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  WallpaperDelegate* wallpaper_delegate = WmShell::Get()->wallpaper_delegate();

  views::Widget* desktop_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "DesktopBackgroundView";
  if (controller->GetWallpaper().isNull())
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = aura_root_window->GetChildById(container_id);
  desktop_widget->Init(params);
  desktop_widget->SetContentsView(
      new LayerControlView(new DesktopBackgroundView()));
  int animation_type = wallpaper_delegate->GetAnimationType();
  WmWindow* desktop_window =
      WmLookup::Get()->GetWindowForWidget(desktop_widget);
  desktop_window->SetVisibilityAnimationType(animation_type);

  RootWindowController* root_window_controller =
      GetRootWindowController(aura_root_window);

  // Enable wallpaper transition for the following cases:
  // 1. Initial(OOBE) wallpaper animation.
  // 2. Wallpaper fades in from a non empty background.
  // 3. From an empty background, chrome transit to a logged in user session.
  // 4. From an empty background, guest user logged in.
  if (wallpaper_delegate->ShouldShowInitialAnimation() ||
      root_window_controller->animating_wallpaper_controller() ||
      WmShell::Get()->GetSessionStateDelegate()->NumberOfLoggedInUsers()) {
    desktop_window->SetVisibilityAnimationTransition(::wm::ANIMATE_SHOW);
    int duration_override = wallpaper_delegate->GetAnimationDurationOverride();
    if (duration_override) {
      desktop_window->SetVisibilityAnimationDuration(
          base::TimeDelta::FromMilliseconds(duration_override));
    }
  } else {
    // Disable animation if transition to login screen from an empty background.
    desktop_window->SetVisibilityAnimationTransition(::wm::ANIMATE_NONE);
  }

  desktop_widget->SetBounds(params.parent->bounds());
  return desktop_widget;
}

}  // namespace ash
