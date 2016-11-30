// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/non_client_frame_controller.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/common/ash_constants.h"
#include "ash/common/ash_layout_constants.h"
#include "ash/common/frame/custom_frame_view_ash.h"
#include "ash/common/wm/panels/panel_frame_view.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/frame/custom_frame_view_mus.h"
#include "ash/mus/frame/detached_title_area_renderer.h"
#include "ash/mus/move_event_handler.h"
#include "ash/mus/property_util.h"
#include "ash/mus/shadow.h"
#include "ash/shared/immersive_fullscreen_controller_delegate.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
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

// Creates a ui::Window to host the top container when in immersive mode. The
// top container contains a DetachedTitleAreaRenderer, which handles drawing and
// events.
class ImmersiveFullscreenControllerDelegateMus
    : public ImmersiveFullscreenControllerDelegate,
      public DetachedTitleAreaRendererHost {
 public:
  ImmersiveFullscreenControllerDelegateMus(views::Widget* frame,
                                           ui::Window* frame_window)
      : frame_(frame), frame_window_(frame_window) {}
  ~ImmersiveFullscreenControllerDelegateMus() override {
    DestroyTitleAreaWindow();
  }

  // WmImmersiveFullscreenControllerDelegate:
  void OnImmersiveRevealStarted() override {
    CreateTitleAreaWindow();
    SetVisibleFraction(0);
  }
  void OnImmersiveRevealEnded() override { DestroyTitleAreaWindow(); }
  void OnImmersiveFullscreenExited() override { DestroyTitleAreaWindow(); }
  void SetVisibleFraction(double visible_fraction) override {
    if (!title_area_window_)
      return;
    gfx::Rect bounds = title_area_window_->bounds();
    bounds.set_y(frame_window_->bounds().y() - bounds.height() +
                 visible_fraction * bounds.height());
    title_area_window_->SetBounds(bounds);
  }
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override {
    std::vector<gfx::Rect> result;
    if (!title_area_window_)
      return result;

    // Clip the bounds of the title area to that of the |frame_window_|.
    gfx::Rect visible_bounds = title_area_window_->bounds();
    visible_bounds.Intersect(frame_window_->bounds());
    // The intersection is in the coordinates of |title_area_window_|'s parent,
    // convert to be in |title_area_window_| and then to screen.
    visible_bounds -= title_area_window_->bounds().origin().OffsetFromOrigin();
    // TODO: this needs updating when parent of |title_area_window| is changed,
    // DCHECK is to ensure when parent changes this code is updated.
    // http://crbug.com/640392.
    DCHECK_EQ(frame_window_->parent(), title_area_window_->parent());
    result.push_back(WmWindowMus::Get(title_area_window_)
                         ->ConvertRectToScreen(visible_bounds));
    return result;
  }

  // DetachedTitleAreaRendererHost:
  void OnDetachedTitleAreaRendererDestroyed(
      DetachedTitleAreaRenderer* renderer) override {
    title_area_renderer_ = nullptr;
    title_area_window_ = nullptr;
  }

 private:
  void CreateTitleAreaWindow() {
    if (title_area_window_)
      return;

    title_area_window_ = frame_window_->window_tree()->NewWindow();
    // TODO(sky): bounds aren't right here. Need to convert to display.
    gfx::Rect bounds = frame_window_->bounds();
    // Use the preferred size as when fullscreen the client area is generally
    // set to 0.
    bounds.set_height(
        NonClientFrameController::GetPreferredClientAreaInsets().top());
    bounds.set_y(bounds.y() - bounds.height());
    title_area_window_->SetBounds(bounds);
    // TODO: making the reveal window a sibling is likely problematic.
    // http://crbug.com/640392, see also comment in GetVisibleBoundsInScreen().
    frame_window_->parent()->AddChild(title_area_window_);
    title_area_window_->Reorder(frame_window_,
                                ui::mojom::OrderDirection::ABOVE);
    title_area_renderer_ =
        new DetachedTitleAreaRenderer(this, frame_, title_area_window_,
                                      DetachedTitleAreaRenderer::Source::MASH);
  }

  void DestroyTitleAreaWindow() {
    if (!title_area_window_)
      return;
    title_area_renderer_->Destroy();
    title_area_renderer_ = nullptr;
    // TitleAreaRevealer ends up owning window.
    title_area_window_ = nullptr;
  }

  // The Widget immersive mode is operating on.
  views::Widget* frame_;

  // The ui::Window associated with |frame_|.
  ui::Window* frame_window_;

  // The window hosting the title area.
  ui::Window* title_area_window_ = nullptr;

  DetachedTitleAreaRenderer* title_area_renderer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveFullscreenControllerDelegateMus);
};

class WmNativeWidgetMus : public views::NativeWidgetMus {
 public:
  WmNativeWidgetMus(views::internal::NativeWidgetDelegate* delegate,
                    ui::Window* window,
                    ui::WindowManagerClient* window_manager_client)
      : NativeWidgetMus(delegate,
                        window,
                        ui::mojom::CompositorFrameSinkType::UNDERLAY),
        window_manager_client_(window_manager_client) {}
  ~WmNativeWidgetMus() override {}

  // NativeWidgetMus:
  views::NonClientFrameView* CreateNonClientFrameView() override {
    move_event_handler_.reset(new MoveEventHandler(
        window(), window_manager_client_, GetNativeView()));
    if (ShouldRemoveStandardFrame(window()))
      return new EmptyDraggableNonClientFrameView();
    if (GetWindowType(window()) == ui::mojom::WindowType::PANEL)
      return new PanelFrameView(GetWidget(), PanelFrameView::FRAME_ASH);
    immersive_delegate_.reset(
        new ImmersiveFullscreenControllerDelegateMus(GetWidget(), window()));
    const bool enable_immersive =
        !window()->HasSharedProperty(
            ui::mojom::WindowManager::kDisableImmersive_Property) ||
        !window()->GetSharedProperty<bool>(
            ui::mojom::WindowManager::kDisableImmersive_Property);
    return new CustomFrameViewMus(GetWidget(), immersive_delegate_.get(),
                                  enable_immersive);
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
    window_tree_host->SetOutputSurfacePaddingInPixels(
        gfx::Insets(inset, inset, inset, inset));
    window_tree_host->window()->layer()->Add(shadow_->layer());
    shadow_->layer()->parent()->StackAtBottom(shadow_->layer());
  }

 private:
  // The shadow, may be null.
  std::unique_ptr<Shadow> shadow_;

  std::unique_ptr<MoveEventHandler> move_event_handler_;

  ui::WindowManagerClient* window_manager_client_;

  std::unique_ptr<ImmersiveFullscreenControllerDelegateMus> immersive_delegate_;

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

// Returns the frame insets to use when ShouldUseExtendedHitRegion() returns
// true.
gfx::Insets GetExtendedHitRegion() {
  return gfx::Insets(kResizeOutsideBoundsSize, kResizeOutsideBoundsSize,
                     kResizeOutsideBoundsSize, kResizeOutsideBoundsSize);
}

}  // namespace

// static
void NonClientFrameController::Create(
    ui::Window* parent,
    ui::Window* window,
    ui::WindowManagerClient* window_manager_client) {
  new NonClientFrameController(parent, window, window_manager_client);
}

// static
gfx::Insets NonClientFrameController::GetPreferredClientAreaInsets() {
  // TODO(sky): figure out a better way to get this rather than hard coding.
  // This value comes from the header (see DefaultHeaderPainter::LayoutHeader,
  // which uses the preferred height of the CaptionButtonContainer, which uses
  // the height of the close button).
  return gfx::Insets(
      GetAshLayoutSize(AshLayoutSize::NON_BROWSER_CAPTION_BUTTON).height(), 0,
      0, 0);
}

// static
int NonClientFrameController::GetMaxTitleBarButtonWidth() {
  // TODO(sky): same comment as for GetPreferredClientAreaInsets().
  return GetAshLayoutSize(AshLayoutSize::NON_BROWSER_CAPTION_BUTTON).width() *
         3;
}

NonClientFrameController::NonClientFrameController(
    ui::Window* parent,
    ui::Window* window,
    ui::WindowManagerClient* window_manager_client)
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
      new WmNativeWidgetMus(widget_, window, window_manager_client);
  widget_->Init(params);

  parent->AddChild(window);

  widget_->ShowInactive();

  const int shadow_inset =
      Shadow::GetInteriorInsetForStyle(Shadow::STYLE_ACTIVE);
  const gfx::Insets extended_hit_region =
      wm_window->ShouldUseExtendedHitRegion() ? GetExtendedHitRegion()
                                              : gfx::Insets();
  window_manager_client->SetUnderlaySurfaceOffsetAndExtendedHitArea(
      window, gfx::Vector2d(shadow_inset, shadow_inset), extended_hit_region);
}

NonClientFrameController::~NonClientFrameController() {
  if (window_)
    window_->RemoveObserver(this);
  if (detached_title_area_renderer_)
    detached_title_area_renderer_->Destroy();
}

void NonClientFrameController::OnDetachedTitleAreaRendererDestroyed(
    DetachedTitleAreaRenderer* renderer) {
  DCHECK_EQ(detached_title_area_renderer_, renderer);
  detached_title_area_renderer_ = nullptr;
}

base::string16 NonClientFrameController::GetWindowTitle() const {
  if (!window_->HasSharedProperty(
          ui::mojom::WindowManager::kWindowTitle_Property)) {
    return base::string16();
  }

  base::string16 title = window_->GetSharedProperty<base::string16>(
      ui::mojom::WindowManager::kWindowTitle_Property);

  if (IsWindowJanky(window_))
    title += base::ASCIIToUTF16(" !! Not responding !!");

  return title;
}

bool NonClientFrameController::CanResize() const {
  return window_ && (::ash::mus::GetResizeBehavior(window_) &
                     ui::mojom::kResizeBehaviorCanResize);
}

bool NonClientFrameController::CanMaximize() const {
  return window_ && (::ash::mus::GetResizeBehavior(window_) &
                     ui::mojom::kResizeBehaviorCanMaximize);
}

bool NonClientFrameController::CanMinimize() const {
  return window_ && (::ash::mus::GetResizeBehavior(window_) &
                     ui::mojom::kResizeBehaviorCanMinimize);
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

void NonClientFrameController::OnTreeChanged(const TreeChangeParams& params) {
  if (params.new_parent != window_ ||
      !ShouldRenderParentTitleArea(params.target)) {
    return;
  }
  if (detached_title_area_renderer_) {
    detached_title_area_renderer_->Destroy();
    detached_title_area_renderer_ = nullptr;
  }
  detached_title_area_renderer_ = new DetachedTitleAreaRenderer(
      this, widget_, params.target, DetachedTitleAreaRenderer::Source::CLIENT);
}

void NonClientFrameController::OnWindowSharedPropertyChanged(
    ui::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name == ui::mojom::WindowManager::kResizeBehavior_Property)
    widget_->OnSizeConstraintsChanged();
  else if (name == ui::mojom::WindowManager::kWindowTitle_Property)
    widget_->UpdateWindowTitle();
}

void NonClientFrameController::OnWindowLocalPropertyChanged(ui::Window* window,
                                                            const void* key,
                                                            intptr_t old) {
  if (IsWindowJankyProperty(key)) {
    widget_->UpdateWindowTitle();
    widget_->non_client_view()->frame_view()->SchedulePaint();
  }
}

void NonClientFrameController::OnWindowDestroyed(ui::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

}  // namespace mus
}  // namespace ash
