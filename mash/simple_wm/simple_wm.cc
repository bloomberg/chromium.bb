// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/simple_wm/simple_wm.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/mojo/geometry.mojom.h"
#include "ui/views/controls/label.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/default_activation_client.h"

namespace simple_wm {

namespace {

const int kNonClientTopHeight = 24;
const int kNonClientSize = 5;

}  // namespace

class SimpleWM::FrameView : public views::WidgetDelegateView,
                            public aura::WindowObserver {
 public:
  explicit FrameView(aura::Window* client_window)
      : client_window_(client_window) {
    client_window_->AddObserver(this);
  }
  ~FrameView() override {}

 private:
  // views::WidgetDelegateView:
  base::string16 GetWindowTitle() const override {
    base::string16* title =
        client_window_->GetProperty(aura::client::kTitleKey);
    if (!title)
      return base::UTF8ToUTF16("(Window)");
    return *title;
  }
  void Layout() override {
    // Client offsets are applied automatically by the window service.
    gfx::Rect parent_bounds = GetWidget()->GetNativeWindow()->bounds();
    parent_bounds.set_origin(gfx::Point());
    client_window_->SetBounds(parent_bounds);
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window, const void* key,
                               intptr_t old) override {
    if (key == aura::client::kTitleKey)
      GetWidget()->UpdateWindowTitle();
  }

  aura::Window* client_window_;

  DISALLOW_COPY_AND_ASSIGN(FrameView);
};

SimpleWM::SimpleWM() {}

SimpleWM::~SimpleWM() {
  // WindowTreeHost uses state from WindowTreeClient, so destroy it first.
  window_tree_host_.reset();

  // WindowTreeClient destruction may callback to us.
  window_tree_client_.reset();

  gpu_service_.reset();

  display::Screen::SetScreenInstance(nullptr);
}

void SimpleWM::OnStart() {
  CHECK(!started_);
  started_ = true;
  screen_ = base::MakeUnique<display::ScreenBase>();
  display::Screen::SetScreenInstance(screen_.get());
  aura_init_ = base::MakeUnique<views::AuraInit>(
      context()->connector(), context()->identity(), "views_mus_resources.pak",
      std::string(), nullptr, views::AuraInit::Mode::AURA_MUS_WINDOW_MANAGER);
  gpu_service_ = ui::GpuService::Create(context()->connector(), nullptr);
  compositor_context_factory_ =
      base::MakeUnique<aura::MusContextFactory>(gpu_service_.get());
  aura::Env::GetInstance()->set_context_factory(
      compositor_context_factory_.get());
  window_tree_client_ = base::MakeUnique<aura::WindowTreeClient>(
      context()->connector(), this, this);
  aura::Env::GetInstance()->SetWindowTreeClient(window_tree_client_.get());
  window_tree_client_->ConnectAsWindowManager();
}

bool SimpleWM::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  return true;
}

void SimpleWM::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  // WindowTreeClients configured as the window manager should never get
  // OnEmbed().
  NOTREACHED();
}

void SimpleWM::OnLostConnection(aura::WindowTreeClient* client) {
  window_tree_host_.reset();
  window_tree_client_.reset();
}

void SimpleWM::OnEmbedRootDestroyed(aura::Window* root) {
  // WindowTreeClients configured as the window manager should never get
  // OnEmbedRootDestroyed().
  NOTREACHED();
}

void SimpleWM::OnPointerEventObserved(const ui::PointerEvent& event,
                                      aura::Window* target) {
  // Don't care.
}

aura::client::CaptureClient* SimpleWM::GetCaptureClient() {
  return wm_state_.capture_controller();
}

aura::PropertyConverter* SimpleWM::GetPropertyConverter() {
  return &property_converter_;
}

void SimpleWM::SetWindowManagerClient(
    aura::WindowManagerClient* client) {
  window_manager_client_ = client;
}

bool SimpleWM::OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) {
  FrameView* frame_view = GetFrameViewForClientWindow(window);
  frame_view->GetWidget()->SetBounds(*bounds);
  return true;
}

bool SimpleWM::OnWmSetProperty(
    aura::Window* window,
    const std::string& name,
    std::unique_ptr<std::vector<uint8_t>>* new_data) {
  return true;
}

aura::Window* SimpleWM::OnWmCreateTopLevelWindow(
    ui::mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  aura::Window* client_window = new aura::Window(nullptr);
  SetWindowType(client_window, window_type);
  client_window->Init(ui::LAYER_NOT_DRAWN);

  views::Widget* frame_widget = new views::Widget;
  views::NativeWidgetAura* frame_native_widget =
      new views::NativeWidgetAura(frame_widget, true);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  FrameView* frame_view = new FrameView(client_window);
  params.delegate = frame_view;
  params.native_widget = frame_native_widget;
  params.parent = root_;
  params.bounds = gfx::Rect(10, 10, 500, 500);
  frame_widget->Init(params);
  frame_widget->Show();

  frame_widget->GetNativeWindow()->AddChild(client_window);

  client_window_to_frame_view_[client_window] = frame_view;
  // TODO(beng): probably need to observe client_window from now on so we can
  // clean up this map.

  return client_window;
}

void SimpleWM::OnWmClientJankinessChanged(
    const std::set<aura::Window*>& client_windows,
    bool janky) {
  // Don't care.
}

void SimpleWM::OnWmWillCreateDisplay(const display::Display& display) {
  screen_->display_list().AddDisplay(display,
                                     display::DisplayList::Type::PRIMARY);
}

void SimpleWM::OnWmNewDisplay(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
    const display::Display& display) {
  // Only handles a single root.
  DCHECK(!root_);
  window_tree_host_ = std::move(window_tree_host);
  window_tree_host_->InitCompositor();
  root_ = window_tree_host_->window();
  DCHECK(window_manager_client_);
  window_manager_client_->AddActivationParent(root_);
  ui::mojom::FrameDecorationValuesPtr frame_decoration_values =
      ui::mojom::FrameDecorationValues::New();
  frame_decoration_values->normal_client_area_insets.Set(
      kNonClientTopHeight, kNonClientSize, kNonClientSize, kNonClientSize);
  frame_decoration_values->max_title_bar_button_width = 0;
  window_manager_client_->SetFrameDecorationValues(
      std::move(frame_decoration_values));
  new wm::DefaultActivationClient(root_);
  aura::client::SetFocusClient(root_, &focus_client_);
}

void SimpleWM::OnWmDisplayRemoved(
    aura::WindowTreeHostMus* window_tree_host) {
  DCHECK_EQ(window_tree_host, window_tree_host_.get());
  root_ = nullptr;
  window_tree_host_.reset();
}

void SimpleWM::OnWmDisplayModified(const display::Display& display) {}

void SimpleWM::OnWmPerformMoveLoop(
    aura::Window* window,
    ui::mojom::MoveLoopSource source,
    const gfx::Point& cursor_location,
    const base::Callback<void(bool)>& on_done) {
  // Don't care.
}

void SimpleWM::OnWmCancelMoveLoop(aura::Window* window) {}

void SimpleWM::OnWmSetClientArea(
    aura::Window* window,
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {}

SimpleWM::FrameView* SimpleWM::GetFrameViewForClientWindow(
    aura::Window* client_window) {
  auto it = client_window_to_frame_view_.find(client_window);
  return it != client_window_to_frame_view_.end() ? it->second : nullptr;
}

}  // namespace simple_wm

