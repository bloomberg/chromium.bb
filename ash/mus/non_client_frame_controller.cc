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
#include "ash/common/wm_window.h"
#include "ash/mus/frame/custom_frame_view_mus.h"
#include "ash/mus/frame/detached_title_area_renderer.h"
#include "ash/mus/move_event_handler.h"
#include "ash/mus/property_util.h"
#include "ash/mus/window_manager.h"
#include "ash/mus/window_properties.h"
#include "ash/shared/immersive_fullscreen_controller_delegate.h"
#include "ash/wm/window_properties.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(ash::mus::NonClientFrameController*);

namespace ash {
namespace mus {
namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(NonClientFrameController*,
                             kNonClientFrameControllerKey,
                             nullptr);

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

// Creates a Window to host the top container when in immersive mode. The
// top container contains a DetachedTitleAreaRenderer, which handles drawing and
// events.
class ImmersiveFullscreenControllerDelegateMus
    : public ImmersiveFullscreenControllerDelegate {
 public:
  ImmersiveFullscreenControllerDelegateMus(views::Widget* frame,
                                           aura::Window* frame_window)
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
    aura::Window* title_area_window = GetTitleAreaWindow();
    if (!title_area_window)
      return;
    gfx::Rect bounds = title_area_window->bounds();
    bounds.set_y(frame_window_->bounds().y() - bounds.height() +
                 visible_fraction * bounds.height());
    title_area_window->SetBounds(bounds);
  }
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override {
    std::vector<gfx::Rect> result;
    const aura::Window* title_area_window = GetTitleAreaWindow();
    if (!title_area_window)
      return result;

    // Clip the bounds of the title area to that of the |frame_window_|.
    gfx::Rect visible_bounds = title_area_window->bounds();
    visible_bounds.Intersect(frame_window_->bounds());
    // The intersection is in the coordinates of |title_area_window|'s parent,
    // convert to be in |title_area_window| and then to screen.
    visible_bounds -= title_area_window->bounds().origin().OffsetFromOrigin();
    // TODO: this needs updating when parent of |title_area_window| is changed,
    // DCHECK is to ensure when parent changes this code is updated.
    // http://crbug.com/640392.
    DCHECK_EQ(frame_window_->parent(), title_area_window->parent());
    result.push_back(
        WmWindow::Get(title_area_window)->ConvertRectToScreen(visible_bounds));
    return result;
  }

 private:
  void CreateTitleAreaWindow() {
    if (GetTitleAreaWindow())
      return;

    // TODO(sky): bounds aren't right here. Need to convert to display.
    gfx::Rect bounds = frame_window_->bounds();
    // Use the preferred size as when fullscreen the client area is generally
    // set to 0.
    bounds.set_height(
        NonClientFrameController::GetPreferredClientAreaInsets().top());
    bounds.set_y(bounds.y() - bounds.height());
    title_area_renderer_ =
        base::MakeUnique<DetachedTitleAreaRendererForInternal>(frame_);
    title_area_renderer_->widget()->SetBounds(bounds);
    title_area_renderer_->widget()->ShowInactive();
  }

  void DestroyTitleAreaWindow() { title_area_renderer_.reset(); }

  aura::Window* GetTitleAreaWindow() {
    return const_cast<aura::Window*>(
        const_cast<const ImmersiveFullscreenControllerDelegateMus*>(this)
            ->GetTitleAreaWindow());
  }
  const aura::Window* GetTitleAreaWindow() const {
    return title_area_renderer_
               ? title_area_renderer_->widget()->GetNativeView()
               : nullptr;
  }

  // The Widget immersive mode is operating on.
  views::Widget* frame_;

  // The ui::Window associated with |frame_|.
  aura::Window* frame_window_;

  std::unique_ptr<DetachedTitleAreaRendererForInternal> title_area_renderer_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveFullscreenControllerDelegateMus);
};

class WmNativeWidgetAura : public views::NativeWidgetAura {
 public:
  WmNativeWidgetAura(views::internal::NativeWidgetDelegate* delegate,
                     aura::WindowManagerClient* window_manager_client,
                     bool remove_standard_frame,
                     bool enable_immersive)
      // The NativeWidget is mirroring the real Widget created in client code.
      // |is_parallel_widget_in_window_manager| is used to indicate this
      : views::NativeWidgetAura(
            delegate,
            true /* is_parallel_widget_in_window_manager */),
        remove_standard_frame_(remove_standard_frame),
        enable_immersive_(enable_immersive),
        window_manager_client_(window_manager_client) {}
  ~WmNativeWidgetAura() override {}

  // views::NativeWidgetAura:
  views::NonClientFrameView* CreateNonClientFrameView() override {
    move_event_handler_ = base::MakeUnique<MoveEventHandler>(
        window_manager_client_, GetNativeView());
    // TODO(sky): investigate why we have this. Seems this should be the same
    // as not specifying client area insets.
    if (remove_standard_frame_)
      return new EmptyDraggableNonClientFrameView();
    aura::Window* window = GetNativeView();
    if (window->GetProperty(aura::client::kWindowTypeKey) ==
        ui::mojom::WindowType::PANEL)
      return new PanelFrameView(GetWidget(), PanelFrameView::FRAME_ASH);
    immersive_delegate_ =
        base::MakeUnique<ImmersiveFullscreenControllerDelegateMus>(GetWidget(),
                                                                   window);
    return new CustomFrameViewMus(GetWidget(), immersive_delegate_.get(),
                                  enable_immersive_);
  }

 private:
  const bool remove_standard_frame_;
  const bool enable_immersive_;

  std::unique_ptr<MoveEventHandler> move_event_handler_;

  aura::WindowManagerClient* window_manager_client_;

  std::unique_ptr<ImmersiveFullscreenControllerDelegateMus> immersive_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WmNativeWidgetAura);
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

    frame_controller_->window_manager_client()->RequestClose(
        frame_controller_->window());
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

NonClientFrameController::NonClientFrameController(
    aura::Window* parent,
    aura::Window* context,
    const gfx::Rect& bounds,
    ui::mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties,
    WindowManager* window_manager)
    : window_manager_client_(window_manager->window_manager_client()),
      widget_(new views::Widget),
      window_(nullptr) {
  // To simplify things this code creates a Widget. While a Widget is created
  // we need to ensure we don't inadvertently change random properties of the
  // underlying ui::Window. For example, showing the Widget shouldn't change
  // the bounds of the ui::Window in anyway.
  //
  // Assertions around InitParams::Type matching ui::mojom::WindowType exist in
  // MusClient.
  views::Widget::InitParams params(
      static_cast<views::Widget::InitParams::Type>(window_type));
  DCHECK((parent && !context) || (!parent && context));
  params.parent = parent;
  params.context = context;
  // TODO: properly set |params.activatable|. Should key off whether underlying
  // (mus) window can have focus.
  params.delegate = this;
  params.bounds = bounds;
  WmNativeWidgetAura* native_widget = new WmNativeWidgetAura(
      widget_, window_manager_client_, ShouldRemoveStandardFrame(*properties),
      ShouldEnableImmersive(*properties));
  window_ = native_widget->GetNativeView();
  window_->SetProperty(aura::client::kTopLevelWindowInWM, true);
  window_->SetProperty(kNonClientFrameControllerKey, this);
  window_->SetProperty(kWidgetCreationTypeKey, WidgetCreationType::FOR_CLIENT);
  window_->AddObserver(this);
  params.native_widget = native_widget;
  aura::SetWindowType(window_, window_type);
  aura::PropertyConverter* property_converter =
      window_manager->property_converter();
  for (auto& property_pair : *properties) {
    property_converter->SetPropertyFromTransportValue(
        window_, property_pair.first, &property_pair.second);
  }
  // Applying properties will have set the show state if specified.
  // NativeWidgetAura resets the show state from |params|, so we need to update
  // |params|.
  params.show_state = window_->GetProperty(aura::client::kShowStateKey);
  widget_->Init(params);
  did_init_native_widget_ = true;

  WmWindow* wm_window = WmWindow::Get(window_);
  const gfx::Insets extended_hit_region =
      wm_window->ShouldUseExtendedHitRegion() ? GetExtendedHitRegion()
                                              : gfx::Insets();
  window_manager_client_->SetExtendedHitArea(window_, extended_hit_region);

  aura::client::GetTransientWindowClient()->AddObserver(this);
}

// static
NonClientFrameController* NonClientFrameController::Get(aura::Window* window) {
  return window->GetProperty(kNonClientFrameControllerKey);
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

void NonClientFrameController::SetClientArea(
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {
  client_area_insets_ = insets;
  additional_client_areas_ = additional_client_areas;
}

NonClientFrameController::~NonClientFrameController() {
  aura::client::GetTransientWindowClient()->RemoveObserver(this);
  if (window_)
    window_->RemoveObserver(this);
}

base::string16 NonClientFrameController::GetWindowTitle() const {
  if (!window_ || !window_->GetProperty(aura::client::kTitleKey))
    return base::string16();

  base::string16 title = *window_->GetProperty(aura::client::kTitleKey);

  if (window_->GetProperty(kWindowIsJanky))
    title += base::ASCIIToUTF16(" !! Not responding !!");

  return title;
}

bool NonClientFrameController::CanResize() const {
  return window_ && WmWindow::Get(window_)->CanResize();
}

bool NonClientFrameController::CanMaximize() const {
  return window_ && WmWindow::Get(window_)->CanMaximize();
}

bool NonClientFrameController::CanMinimize() const {
  return window_ && WmWindow::Get(window_)->CanMinimize();
}

bool NonClientFrameController::ShouldShowWindowTitle() const {
  // Only draw the title if the client hasn't declared any additional client
  // areas which might conflict with it.
  return window_ && additional_client_areas_.empty();
}

views::ClientView* NonClientFrameController::CreateClientView(
    views::Widget* widget) {
  return new ClientViewMus(widget, GetContentsView(), this);
}

void NonClientFrameController::OnWindowPropertyChanged(aura::Window* window,
                                                       const void* key,
                                                       intptr_t old) {
  // Properties are applied before the call to InitNativeWidget(). Ignore
  // processing changes in this case as the Widget is not in a state where we
  // can use it yet.
  if (!did_init_native_widget_)
    return;

  if (key == kWindowIsJanky) {
    widget_->UpdateWindowTitle();
    widget_->non_client_view()->frame_view()->SchedulePaint();
  } else if (key == aura::client::kResizeBehaviorKey) {
    widget_->OnSizeConstraintsChanged();
  } else if (key == aura::client::kTitleKey) {
    widget_->UpdateWindowTitle();
  }
}

void NonClientFrameController::OnWindowDestroyed(aura::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

void NonClientFrameController::OnTransientChildWindowAdded(
    aura::Window* parent,
    aura::Window* transient_child) {
  if (parent != window_ ||
      !transient_child->GetProperty(kRenderTitleAreaProperty)) {
    return;
  }

  DetachedTitleAreaRendererForClient* renderer =
      DetachedTitleAreaRendererForClient::ForWindow(transient_child);
  if (!renderer || renderer->is_attached())
    return;

  renderer->Attach(widget_);
}

void NonClientFrameController::OnTransientChildWindowRemoved(
    aura::Window* parent,
    aura::Window* transient_child) {
  if (parent != window_)
    return;

  DetachedTitleAreaRendererForClient* renderer =
      DetachedTitleAreaRendererForClient::ForWindow(transient_child);
  if (renderer)
    renderer->Detach();
}

void NonClientFrameController::OnWillRestackTransientChildAbove(
    aura::Window* parent,
    aura::Window* transient_child) {}

void NonClientFrameController::OnDidRestackTransientChildAbove(
    aura::Window* parent,
    aura::Window* transient_child) {}

}  // namespace mus
}  // namespace ash
