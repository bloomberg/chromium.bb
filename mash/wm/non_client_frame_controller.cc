// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/non_client_frame_controller.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mash/wm/bridge/wm_window_mus.h"
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
#include "ui/gfx/geometry/vector2d.h"
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
                    shell::Connector* connector,
                    mus::Window* window,
                    mus::WindowManagerClient* window_manager_client)
      : NativeWidgetMus(delegate,
                        connector,
                        window,
                        mus::mojom::SurfaceType::UNDERLAY),
        window_manager_client_(window_manager_client) {}
  ~WmNativeWidgetMus() override {
  }

  // NativeWidgetMus:
  views::NonClientFrameView* CreateNonClientFrameView() override {
    views::Widget* widget =
        static_cast<views::internal::NativeWidgetPrivate*>(this)->GetWidget();
    NonClientFrameViewMash* frame_view =
        new NonClientFrameViewMash(widget, window());
    move_event_handler_.reset(new MoveEventHandler(
        window(), window_manager_client_, GetNativeView()));
    return frame_view;
  }
  void InitNativeWidget(const views::Widget::InitParams& params) override {
    views::NativeWidgetMus::InitNativeWidget(params);
    aura::WindowTreeHost* window_tree_host = GetNativeView()->GetHost();
    // TODO(sky): shadow should be determined by window type and shadow type.
    shadow_.reset(new Shadow);
    shadow_->Init(Shadow::STYLE_INACTIVE);
    shadow_->Install(window());
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
  bool SetWindowTitle(const base::string16& title) override {
    // Do nothing. The client controls the window title, not us.
    return false;
  }
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override {
    // Do nothing. The client controls window icons, not us.
  }
  void UpdateClientArea() override {
    // This pushes the client area to the WS. We don't want to do that as
    // the client area should come from the client, not us.
  }

 private:
  // The shadow, may be null.
  std::unique_ptr<Shadow> shadow_;

  std::unique_ptr<MoveEventHandler> move_event_handler_;

  mus::WindowManagerClient* window_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(WmNativeWidgetMus);
};

class ClientViewMus : public views::ClientView {
 public:
  ClientViewMus(views::Widget* widget,
                views::View* contents_view,
                NonClientFrameController* frame_controller)
      : views::ClientView(widget, contents_view),
        frame_controller_(frame_controller) {}
  ~ClientViewMus() override {}

  // views::ClientView:
  bool CanClose() override {
    if (!frame_controller_->window())
      return true;

    frame_controller_->window()->RequestClose();
    return false;
  }

 private:
  NonClientFrameController* frame_controller_;

  DISALLOW_COPY_AND_ASSIGN(ClientViewMus);
};

}  // namespace

// static
void NonClientFrameController::Create(
    shell::Connector* connector,
    mus::Window* parent,
    mus::Window* window,
    mus::WindowManagerClient* window_manager_client) {
  new NonClientFrameController(connector, parent, window,
                               window_manager_client);
}

// static
gfx::Insets NonClientFrameController::GetPreferredClientAreaInsets() {
  return NonClientFrameViewMash::GetPreferredClientAreaInsets();
}

// static
int NonClientFrameController::GetMaxTitleBarButtonWidth() {
  return NonClientFrameViewMash::GetMaxTitleBarButtonWidth();
}

NonClientFrameController::NonClientFrameController(
    shell::Connector* connector,
    mus::Window* parent,
    mus::Window* window,
    mus::WindowManagerClient* window_manager_client)
    : widget_(new views::Widget), window_(window) {
  WmWindowMus::Get(window)->set_widget(widget_);
  window_->AddObserver(this);

  // To simplify things this code creates a Widget. While a Widget is created
  // we need to ensure we don't inadvertently change random properties of the
  // underlying mus::Window. For example, showing the Widget shouldn't change
  // the bounds of the mus::Window in anyway.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  // We initiate focus at the mus level, not at the views level.
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.delegate = this;
  params.native_widget =
      new WmNativeWidgetMus(widget_, connector, window, window_manager_client);
  widget_->Init(params);

  parent->AddChild(window);

  widget_->ShowInactive();

  const int shadow_inset =
      Shadow::GetInteriorInsetForStyle(Shadow::STYLE_ACTIVE);
  window_manager_client->SetUnderlaySurfaceOffsetAndExtendedHitArea(
      window, gfx::Vector2d(shadow_inset, shadow_inset),
      FrameBorderHitTestController::GetResizeOutsideBoundsSize());
}

NonClientFrameController::~NonClientFrameController() {
  if (window_)
    window_->RemoveObserver(this);
}

base::string16 NonClientFrameController::GetWindowTitle() const {
  if (!window_->HasSharedProperty(
          mus::mojom::WindowManager::kWindowTitle_Property)) {
    return base::string16();
  }

  return window_->GetSharedProperty<base::string16>(
      mus::mojom::WindowManager::kWindowTitle_Property);
}

views::View* NonClientFrameController::GetContentsView() {
  return this;
}

bool NonClientFrameController::CanResize() const {
  return window_ &&
         (GetResizeBehavior(window_) & mus::mojom::kResizeBehaviorCanResize) !=
             0;
}

bool NonClientFrameController::CanMaximize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          mus::mojom::kResizeBehaviorCanMaximize) != 0;
}

bool NonClientFrameController::CanMinimize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          mus::mojom::kResizeBehaviorCanMinimize) != 0;
}

bool NonClientFrameController::ShouldShowWindowTitle() const {
  // Only draw the title if the client hasn't declared any additional client
  // areas which might conflict with it.
  return window_ && window_->additional_client_areas().empty();
}

views::ClientView* NonClientFrameController::CreateClientView(
    views::Widget* widget) {
  return new ClientViewMus(widget, GetContentsView(), this);
}

void NonClientFrameController::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name == mus::mojom::WindowManager::kResizeBehavior_Property)
    widget_->OnSizeConstraintsChanged();
  else if (name == mus::mojom::WindowManager::kWindowTitle_Property)
    widget_->UpdateWindowTitle();
}

void NonClientFrameController::OnWindowDestroyed(mus::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

}  // namespace wm
}  // namespace mash
