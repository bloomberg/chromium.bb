// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/window_manager_application.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "components/mus/common/util.h"
#include "components/mus/public/cpp/event_matcher.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_host_factory.h"
#include "mash/wm/accelerator_registrar_impl.h"
#include "mash/wm/background_layout.h"
#include "mash/wm/shadow_controller.h"
#include "mash/wm/shelf_layout.h"
#include "mash/wm/user_window_controller_impl.h"
#include "mash/wm/window_layout.h"
#include "mash/wm/window_manager_impl.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "ui/mojo/init/ui_init.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/display_converter.h"

namespace mash {
namespace wm {
namespace {
const uint32_t kWindowSwitchCmd = 1;

void AssertTrue(bool success) {
  DCHECK(success);
}

}  // namespace

WindowManagerApplication::WindowManagerApplication()
    : root_(nullptr),
      window_count_(0),
      app_(nullptr),
      host_client_binding_(this) {}

WindowManagerApplication::~WindowManagerApplication() {}

mus::Window* WindowManagerApplication::GetWindowForContainer(
    mojom::Container container) {
  const mus::Id window_id = root_->connection()->GetConnectionId() << 16 |
                            static_cast<uint16_t>(container);
  return root_->GetChildById(window_id);
}

mus::Window* WindowManagerApplication::GetWindowById(mus::Id id) {
  return root_->GetChildById(id);
}

bool WindowManagerApplication::WindowIsContainer(
    const mus::Window* window) const {
  return window && window->parent() == root_;
}

void WindowManagerApplication::AddAccelerators() {
  window_tree_host_->AddAccelerator(
      kWindowSwitchCmd,
      mus::CreateKeyMatcher(mus::mojom::KeyboardCode::TAB,
                            mus::mojom::kEventFlagControlDown),
      base::Bind(&AssertTrue));
}

void WindowManagerApplication::OnAcceleratorRegistrarDestroyed(
    AcceleratorRegistrarImpl* registrar) {
  accelerator_registrars_.erase(registrar);
}

void WindowManagerApplication::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
  tracing_.Initialize(app);
  window_manager_.reset(new WindowManagerImpl());
  // Don't bind to the WindowManager immediately. Wait for OnEmbed() first.
  mus::mojom::WindowManagerPtr window_manager;
  requests_.push_back(
      make_scoped_ptr(new mojo::InterfaceRequest<mus::mojom::WindowManager>(
          mojo::GetProxy(&window_manager))));
  user_window_controller_.reset(new UserWindowControllerImpl());
  mus::CreateSingleWindowTreeHost(
      app, host_client_binding_.CreateInterfacePtrAndBind(), this,
      &window_tree_host_, std::move(window_manager), window_manager_.get());
}

bool WindowManagerApplication::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<mash::wm::mojom::UserWindowController>(this);
  connection->AddService<mus::mojom::AcceleratorRegistrar>(this);
  connection->AddService<mus::mojom::WindowManager>(this);
  return true;
}

void WindowManagerApplication::OnAccelerator(uint32_t id,
                                             mus::mojom::EventPtr event) {
  switch (id) {
    case kWindowSwitchCmd:
      window_tree_host_->ActivateNextWindow();
      break;
    default:
      for (auto* registrar : accelerator_registrars_) {
        if (registrar->OwnsAccelerator(id)) {
          registrar->ProcessAccelerator(id, std::move(event));
          break;
        }
      }
  }
}

void WindowManagerApplication::OnEmbed(mus::Window* root) {
  root_ = root;
  root_->AddObserver(this);
  CreateContainers();
  background_layout_.reset(new BackgroundLayout(
      GetWindowForContainer(mojom::Container::USER_BACKGROUND)));
  shelf_layout_.reset(
      new ShelfLayout(GetWindowForContainer(mojom::Container::USER_SHELF)));

  mus::Window* window = GetWindowForContainer(mojom::Container::USER_WINDOWS);
  window_layout_.reset(
      new WindowLayout(GetWindowForContainer(mojom::Container::USER_WINDOWS)));
  window_tree_host_->AddActivationParent(window->id());
  window_tree_host_->SetTitle("Mash");

  AddAccelerators();

  ui_init_.reset(new ui::mojo::UIInit(views::GetDisplaysFromWindow(root)));
  aura_init_.reset(new views::AuraInit(app_, "mash_wm_resources.pak"));
  window_manager_->Initialize(this);

  for (auto& request : requests_)
    window_manager_binding_.AddBinding(window_manager_.get(),
                                       std::move(*request));
  requests_.clear();

  user_window_controller_->Initialize(this);
  for (auto& request : user_window_controller_requests_)
    user_window_controller_binding_.AddBinding(user_window_controller_.get(),
                                               std::move(*request));
  user_window_controller_requests_.clear();

  shadow_controller_.reset(new ShadowController(root->connection()));
}

void WindowManagerApplication::OnConnectionLost(
    mus::WindowTreeConnection* connection) {
  // TODO(sky): shutdown.
  NOTIMPLEMENTED();
  shadow_controller_.reset();
}

void WindowManagerApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mash::wm::mojom::UserWindowController> request) {
  if (root_) {
    user_window_controller_binding_.AddBinding(user_window_controller_.get(),
                                               std::move(request));
  } else {
    user_window_controller_requests_.push_back(make_scoped_ptr(
        new mojo::InterfaceRequest<mash::wm::mojom::UserWindowController>(
            std::move(request))));
  }
}

void WindowManagerApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mus::mojom::AcceleratorRegistrar> request) {
  static int accelerator_registrar_count = 0;
  if (accelerator_registrar_count == std::numeric_limits<int>::max()) {
    // Restart from zero if we have reached the limit. It is technically
    // possible to end up with multiple active registrars with the same
    // namespace, but it is highly unlikely. In the event that multiple
    // registrars have the same namespace, this new registrar will be unable to
    // install accelerators.
    accelerator_registrar_count = 0;
  }
  accelerator_registrars_.insert(new AcceleratorRegistrarImpl(
      window_tree_host_.get(), ++accelerator_registrar_count,
      std::move(request),
      base::Bind(&WindowManagerApplication::OnAcceleratorRegistrarDestroyed,
                 base::Unretained(this))));
}

void WindowManagerApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mus::mojom::WindowManager> request) {
  if (root_) {
    window_manager_binding_.AddBinding(window_manager_.get(),
                                       std::move(request));
  } else {
    requests_.push_back(
        make_scoped_ptr(new mojo::InterfaceRequest<mus::mojom::WindowManager>(
            std::move(request))));
  }
}

void WindowManagerApplication::OnWindowDestroyed(mus::Window* window) {
  DCHECK_EQ(window, root_);
  root_->RemoveObserver(this);
  // Delete the |window_manager_| here so that WindowManager doesn't have to
  // worry about the possibility of |root_| being null.
  window_manager_.reset();
  root_ = nullptr;
}

void WindowManagerApplication::CreateContainers() {
  for (uint16_t container =
           static_cast<uint16_t>(mojom::Container::ALL_USER_BACKGROUND);
       container < static_cast<uint16_t>(mojom::Container::COUNT);
       ++container) {
    mus::Window* window = root_->connection()->NewWindow();
    DCHECK_EQ(mus::LoWord(window->id()), container)
        << "Containers must be created before other windows!";
    window->SetBounds(root_->bounds());
    window->SetVisible(true);
    root_->AddChild(window);
  }
}

}  // namespace wm
}  // namespace mash
