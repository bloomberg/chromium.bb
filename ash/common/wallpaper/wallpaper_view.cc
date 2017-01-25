// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wallpaper/wallpaper_view.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wallpaper/wallpaper_widget_controller.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
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
    view->SetPaintToLayer();
  }

  // Overrides views::View.
  void Layout() override {
    WmWindow* window = WmLookup::Get()->GetWindowForWidget(GetWidget());
    // Keep |this| at the bottom since there may be other windows on top of the
    // wallpaper view such as an overview mode shield.
    window->GetParent()->StackChildAtBottom(window);
    display::Display display = window->GetDisplayNearestWindow();

    // TODO(mash): Mash returns a fake ManagedDisplayInfo. crbug.com/622480
    float ui_scale = 1.f;
    display::ManagedDisplayInfo info =
        WmShell::Get()->GetDisplayInfo(display.id());
    if (info.id() == display.id())
      ui_scale = info.GetEffectiveUIScale();

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
// WallpaperView, public:

WallpaperView::WallpaperView()
    : pre_dispatch_handler_(new PreEventDispatchHandler()) {
  set_context_menu_controller(this);
  AddPreTargetHandler(pre_dispatch_handler_.get());
}

WallpaperView::~WallpaperView() {
  RemovePreTargetHandler(pre_dispatch_handler_.get());
}

////////////////////////////////////////////////////////////////////////////////
// WallpaperView, views::View overrides:

void WallpaperView::OnPaint(gfx::Canvas* canvas) {
  // Scale the image while maintaining the aspect ratio, cropping as necessary
  // to fill the wallpaper. Ideally the image should be larger than the largest
  // display supported, if not we will scale and center it if the layout is
  // wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED.
  WallpaperController* controller = WmShell::Get()->wallpaper_controller();
  gfx::ImageSkia wallpaper = controller->GetWallpaper();
  wallpaper::WallpaperLayout layout = controller->GetWallpaperLayout();

  // Wallpapers with png format could be partially transparent. Fill the canvas
  // with black to make it opaque before painting the wallpaper.
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

bool WallpaperView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void WallpaperView::ShowContextMenuForView(views::View* source,
                                           const gfx::Point& point,
                                           ui::MenuSourceType source_type) {
  WmShell::Get()->ShowContextMenu(point, source_type);
}

views::Widget* CreateWallpaper(WmWindow* root_window, int container_id) {
  WallpaperController* controller = WmShell::Get()->wallpaper_controller();
  WallpaperDelegate* wallpaper_delegate = WmShell::Get()->wallpaper_delegate();

  views::Widget* wallpaper_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "WallpaperView";
  if (controller->GetWallpaper().isNull())
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  root_window->GetRootWindowController()->ConfigureWidgetInitParamsForContainer(
      wallpaper_widget, container_id, &params);
  wallpaper_widget->Init(params);
  wallpaper_widget->SetContentsView(new LayerControlView(new WallpaperView()));
  int animation_type = wallpaper_delegate->GetAnimationType();
  WmWindow* wallpaper_window =
      WmLookup::Get()->GetWindowForWidget(wallpaper_widget);
  wallpaper_window->SetVisibilityAnimationType(animation_type);

  RootWindowController* root_window_controller =
      root_window->GetRootWindowController();

  // Enable wallpaper transition for the following cases:
  // 1. Initial(OOBE) wallpaper animation.
  // 2. Wallpaper fades in from a non empty background.
  // 3. From an empty background, chrome transit to a logged in user session.
  // 4. From an empty background, guest user logged in.
  if (wallpaper_delegate->ShouldShowInitialAnimation() ||
      root_window_controller->animating_wallpaper_widget_controller() ||
      WmShell::Get()->GetSessionStateDelegate()->NumberOfLoggedInUsers()) {
    wallpaper_window->SetVisibilityAnimationTransition(::wm::ANIMATE_SHOW);
    int duration_override = wallpaper_delegate->GetAnimationDurationOverride();
    if (duration_override) {
      wallpaper_window->SetVisibilityAnimationDuration(
          base::TimeDelta::FromMilliseconds(duration_override));
    }
  } else {
    // Disable animation if transition to login screen from an empty background.
    wallpaper_window->SetVisibilityAnimationTransition(::wm::ANIMATE_NONE);
  }

  WmWindow* container = root_window->GetChildByShellWindowId(container_id);
  wallpaper_widget->SetBounds(container->GetBounds());
  return wallpaper_widget;
}

}  // namespace ash
