// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/non_client_frame_controller.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/frame/frame_border_hit_test_controller.h"
#include "ash/mus/frame/move_event_handler.h"
#include "ash/mus/frame/non_client_frame_view_mash.h"
#include "ash/mus/property_util.h"
#include "ash/mus/shadow.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/public/interfaces/window_tree_host.mojom.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace mus {
namespace {

// LayoutManager associated with the window created by WindowTreeHost. Resizes
// all children of the parent to match the bounds of the parent. Additionally
// handles sizing of a Shadow.
class ContentWindowLayoutManager : public aura::LayoutManager {
 public:
  ContentWindowLayoutManager(aura::Window* window, Shadow* shadow)
      : window_(window), shadow_(shadow) {
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
  Shadow* shadow_;

  DISALLOW_COPY_AND_ASSIGN(ContentWindowLayoutManager);
};

// This class supports draggable app windows that paint their own custom frames.
// It uses empty insets, doesn't paint anything, and hit tests return HTCAPTION.
class EmptyDraggableNonClientFrameView : public views::NonClientFrameView {
 public:
  EmptyDraggableNonClientFrameView() {}
  ~EmptyDraggableNonClientFrameView() override {}

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return bounds();
  }
  int NonClientHitTest(const gfx::Point& point) override { return HTCAPTION; }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyDraggableNonClientFrameView);
};

class WmNativeWidgetMus : public views::NativeWidgetMus {
 public:
  WmNativeWidgetMus(views::internal::NativeWidgetDelegate* delegate,
                    shell::Connector* connector,
                    ::ui::Window* window,
                    ::ui::WindowManagerClient* window_manager_client)
      : NativeWidgetMus(delegate,
                        connector,
                        window,
                        ::ui::mojom::SurfaceType::UNDERLAY),
        window_manager_client_(window_manager_client) {}
  ~WmNativeWidgetMus() override {}

  // NativeWidgetMus:
  views::NonClientFrameView* CreateNonClientFrameView() override {
    move_event_handler_.reset(new MoveEventHandler(
        window(), window_manager_client_, GetNativeView()));
    if (ShouldRemoveStandardFrame(window()))
      return new EmptyDraggableNonClientFrameView();
    return new NonClientFrameViewMash(GetWidget(), window());
  }
  void InitNativeWidget(const views::Widget::InitParams& params) override {
    views::NativeWidgetMus::InitNativeWidget(params);
    aura::WindowTreeHost* window_tree_host = GetNativeView()->GetHost();
    // TODO(sky): shadow should be determined by window type and shadow type.
    shadow_.reset(new Shadow);
    shadow_->Init(Shadow::STYLE_INACTIVE);
    shadow_->Install(window());
    ContentWindowLayoutManager* layout_manager = new ContentWindowLayoutManager(
        window_tree_host->window(), shadow_.get());
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

  ::ui::WindowManagerClient* window_manager_client_;

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
    ::ui::Window* parent,
    ::ui::Window* window,
    ::ui::WindowManagerClient* window_manager_client) {
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
    ::ui::Window* parent,
    ::ui::Window* window,
    ::ui::WindowManagerClient* window_manager_client)
    : widget_(new views::Widget), window_(window) {
  WmWindowMus* wm_window = WmWindowMus::Get(window);
  wm_window->set_widget(widget_, WmWindowMus::WidgetCreationType::FOR_CLIENT);
  window_->AddObserver(this);

  // To simplify things this code creates a Widget. While a Widget is created
  // we need to ensure we don't inadvertently change random properties of the
  // underlying ui::Window. For example, showing the Widget shouldn't change
  // the bounds of the ui::Window in anyway.
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
  const gfx::Insets extended_hit_region =
      wm_window->ShouldUseExtendedHitRegion()
          ? FrameBorderHitTestController::GetResizeOutsideBoundsSize()
          : gfx::Insets();
  window_manager_client->SetUnderlaySurfaceOffsetAndExtendedHitArea(
      window, gfx::Vector2d(shadow_inset, shadow_inset), extended_hit_region);
}

NonClientFrameController::~NonClientFrameController() {
  if (window_)
    window_->RemoveObserver(this);
}

base::string16 NonClientFrameController::GetWindowTitle() const {
  if (!window_->HasSharedProperty(
          ::ui::mojom::WindowManager::kWindowTitle_Property)) {
    return base::string16();
  }

  base::string16 title = window_->GetSharedProperty<base::string16>(
      ::ui::mojom::WindowManager::kWindowTitle_Property);

  if (IsWindowJanky(window_))
    title += base::ASCIIToUTF16(" !! Not responding !!");

  return title;
}

views::View* NonClientFrameController::GetContentsView() {
  return this;
}

bool NonClientFrameController::CanResize() const {
  return window_ &&
         (GetResizeBehavior(window_) & ::ui::mojom::kResizeBehaviorCanResize) !=
             0;
}

bool NonClientFrameController::CanMaximize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          ::ui::mojom::kResizeBehaviorCanMaximize) != 0;
}

bool NonClientFrameController::CanMinimize() const {
  return window_ &&
         (GetResizeBehavior(window_) &
          ::ui::mojom::kResizeBehaviorCanMinimize) != 0;
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
    ::ui::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name == ::ui::mojom::WindowManager::kResizeBehavior_Property)
    widget_->OnSizeConstraintsChanged();
  else if (name == ::ui::mojom::WindowManager::kWindowTitle_Property)
    widget_->UpdateWindowTitle();
}

void NonClientFrameController::OnWindowLocalPropertyChanged(
    ::ui::Window* window,
    const void* key,
    intptr_t old) {
  if (IsWindowJankyProperty(key)) {
    widget_->UpdateWindowTitle();
    widget_->non_client_view()->frame_view()->SchedulePaint();
  }
}

void NonClientFrameController::OnWindowDestroyed(::ui::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

}  // namespace mus
}  // namespace ash
