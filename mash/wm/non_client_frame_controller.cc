// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/non_client_frame_controller.h"

#include "components/mus/public/cpp/window.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mash/wm/frame/frame_border_hit_test_controller.h"
#include "mash/wm/frame/move_event_handler.h"
#include "mash/wm/frame/non_client_frame_view_mash.h"
#include "mash/wm/property_util.h"
#include "mash/wm/shadow.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget.h"

namespace mash {
namespace wm {
namespace {

enum class ShadowStyle {
  NORMAL,
  SMALL,
};

// LayoutManager associated with the window created by WindowTreeHost. Resizes
// all children of the parent to match the bounds of the parent. Additionally
// handles sizing of a Shadow.
class ContentWindowLayoutManager : public aura::LayoutManager {
 public:
  explicit ContentWindowLayoutManager(aura::Window* window,
                                      ShadowStyle style,
                                      Shadow* shadow)
      : window_(window), style_(style), shadow_(shadow) {
    OnWindowResized();
  }
  ~ContentWindowLayoutManager() override {}

  void SetShadow(Shadow* shadow) {
    shadow_ = shadow;
    if (shadow_)
      shadow_->SetContentBounds(child_bounds());
  }

 private:
  // Bounds for child windows.
  gfx::Rect child_bounds() const {
    return gfx::Rect(0, 0, window_->bounds().size().width(),
                     window_->bounds().size().height());
  }

  void UpdateChildBounds(aura::Window* child) {
    child->SetBounds(child_bounds());
  }

  // aura::LayoutManager:
  void OnWindowResized() override {
    for (aura::Window* child : window_->children())
      UpdateChildBounds(child);
    // Shadow takes care of resizing the layer appropriately.
    if (shadow_)
      shadow_->SetContentBounds(child_bounds());
  }
  void OnWindowAddedToLayout(aura::Window* child) override {
    UpdateChildBounds(child);
  }
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* window_;
  const ShadowStyle style_;
  Shadow* shadow_;

  DISALLOW_COPY_AND_ASSIGN(ContentWindowLayoutManager);
};

class WmNativeWidgetMus : public views::NativeWidgetMus {
 public:
  WmNativeWidgetMus(views::internal::NativeWidgetDelegate* delegate,
                    mojo::Shell* shell,
                    mus::Window* window)
      : NativeWidgetMus(delegate,
                        shell,
                        window,
                        mus::mojom::SURFACE_TYPE_UNDERLAY) {}
  ~WmNativeWidgetMus() override {
    if (move_event_handler_) {
      GetNativeView()->GetHost()->window()->RemovePreTargetHandler(
          move_event_handler_.get());
    }
  }

  // NativeWidgetMus:
  views::NonClientFrameView* CreateNonClientFrameView() override {
    views::Widget* widget =
        static_cast<views::internal::NativeWidgetPrivate*>(this)->GetWidget();
    NonClientFrameViewMash* frame_view =
        new NonClientFrameViewMash(widget, window());
    aura::Window* root_window = GetNativeView()->GetHost()->window();
    move_event_handler_.reset(new MoveEventHandler(window(), GetNativeView()));
    root_window->AddPreTargetHandler(move_event_handler_.get());
    return frame_view;
  }
  void InitNativeWidget(const views::Widget::InitParams& params) override {
    views::NativeWidgetMus::InitNativeWidget(params);
    aura::WindowTreeHost* window_tree_host = GetNativeView()->GetHost();
    // TODO(sky): shadow should be determined by window type.
    shadow_.reset(new Shadow);
    shadow_->Init(Shadow::STYLE_INACTIVE);
    SetShadow(window(), shadow_.get());
    ContentWindowLayoutManager* layout_manager = new ContentWindowLayoutManager(
        window_tree_host->window(), ShadowStyle::NORMAL, shadow_.get());
    window_tree_host->window()->SetLayoutManager(layout_manager);
    const int inset = Shadow::GetInteriorInsetForStyle(Shadow::STYLE_ACTIVE);
    window_tree_host->SetOutputSurfacePadding(
        gfx::Insets(inset, inset, inset, inset));
    window_tree_host->window()->layer()->Add(shadow_->layer());
    shadow_->layer()->parent()->StackAtBottom(shadow_->layer());
  }
  void CenterWindow(const gfx::Size& size) override {
    // Do nothing. The client controls the size, not us.
  }
  void UpdateClientArea() override {
    // This pushes the client area to the WS. We don't want to do that as
    // the client area should come from the client, not us.
  }

 private:
  // The shadow, may be null.
  scoped_ptr<Shadow> shadow_;

  scoped_ptr<MoveEventHandler> move_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WmNativeWidgetMus);
};

}  // namespace

NonClientFrameController::NonClientFrameController(
    mojo::Shell* shell,
    mus::Window* window,
    mus::mojom::WindowTreeHost* window_tree_host)
    : widget_(new views::Widget),
      window_(window),
      mus_window_tree_host_(window_tree_host) {
  window_->AddObserver(this);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.native_widget = new WmNativeWidgetMus(widget_, shell, window);
  widget_->Init(params);
  widget_->Show();

  const int shadow_inset =
      Shadow::GetInteriorInsetForStyle(Shadow::STYLE_ACTIVE);
  mus_window_tree_host_->SetUnderlaySurfaceOffsetAndExtendedHitArea(
      window->id(), shadow_inset, shadow_inset,
      mojo::Insets::From(
          FrameBorderHitTestController::GetResizeOutsideBoundsSize()));
}

// static
gfx::Insets NonClientFrameController::GetPreferredClientAreaInsets() {
  return NonClientFrameViewMash::GetPreferredClientAreaInsets();
}

// static
int NonClientFrameController::GetMaxTitleBarButtonWidth() {
  return NonClientFrameViewMash::GetMaxTitleBarButtonWidth();
}

NonClientFrameController::~NonClientFrameController() {
  if (window_)
    window_->RemoveObserver(this);
}

views::View* NonClientFrameController::GetContentsView() {
  return this;
}

bool NonClientFrameController::CanResize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          mus::mojom::RESIZE_BEHAVIOR_CAN_RESIZE) != 0;
}

bool NonClientFrameController::CanMaximize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          mus::mojom::RESIZE_BEHAVIOR_CAN_MAXIMIZE) != 0;
}

bool NonClientFrameController::CanMinimize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          mus::mojom::RESIZE_BEHAVIOR_CAN_MINIMIZE) != 0;
}

void NonClientFrameController::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name == mus::mojom::WindowManager::kResizeBehavior_Property)
    widget_->OnSizeConstraintsChanged();
}

void NonClientFrameController::OnWindowDestroyed(mus::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

}  // namespace wm
}  // namespace mash
