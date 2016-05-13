// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_manager_factory.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "ui/display/display.h"
#include "ui/mojo/display/display_type_converters.h"

namespace mus {
namespace test {

class TestWM : public shell::ShellClient,
               public mus::mojom::WindowManagerFactory,
               public mus::WindowTreeDelegate,
               public mus::WindowManagerDelegate {
 public:
  TestWM() : window_manager_factory_binding_(this) {}
  ~TestWM() override {}

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override {
    mus::mojom::WindowManagerFactoryServicePtr wm_factory_service;
    connector->ConnectToInterface("mojo:mus", &wm_factory_service);
    wm_factory_service->SetWindowManagerFactory(
        window_manager_factory_binding_.CreateInterfacePtrAndBind());
  }
  bool AcceptConnection(shell::Connection* connection) override {
    return true;
  }

  // mus::mojom::WindowManagerFactory:
  void CreateWindowManager(
      mus::mojom::DisplayPtr display,
      mus::mojom::WindowTreeClientRequest request) override {
    mus::WindowTreeConnection::CreateForWindowManager(
        this, std::move(request),
        mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED, this);
  }

  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override {
    root_ = root;
    mus::mojom::FrameDecorationValuesPtr frame_decoration_values =
        mus::mojom::FrameDecorationValues::New();
    frame_decoration_values->normal_client_area_insets =
        mojo::Insets::From(gfx::Insets());
    frame_decoration_values->maximized_client_area_insets =
        mojo::Insets::From(gfx::Insets());
    frame_decoration_values->max_title_bar_button_width = 0;
    window_manager_client_->SetFrameDecorationValues(
        std::move(frame_decoration_values));
  }
  void OnConnectionLost(mus::WindowTreeConnection* connection) override {
  }
  void OnEventObserved(const ui::Event& event, mus::Window* target) override {
    // Don't care.
  }

  // mus::WindowManagerDelegate:
  void SetWindowManagerClient(mus::WindowManagerClient* client) override {
    window_manager_client_ = client;
  }
  bool OnWmSetBounds(mus::Window* window, gfx::Rect* bounds) override {
    return true;
  }
  bool OnWmSetProperty(
      mus::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override {
    return true;
  }
  mus::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override {
    mus::Window* window = root_->connection()->NewWindow(properties);
    window->SetBounds(gfx::Rect(10, 10, 500, 500));
    root_->AddChild(window);
    return window;
  }
  void OnAccelerator(uint32_t id, const ui::Event& event) override {
    // Don't care.
  }

  mojo::Binding<mus::mojom::WindowManagerFactory>
      window_manager_factory_binding_;
  mus::Window* root_ = nullptr;
  mus::WindowManagerClient* window_manager_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestWM);
};

}  // namespace test
}  // namespace mus

MojoResult MojoMain(MojoHandle shell_handle) {
  shell::ApplicationRunner runner(new mus::test::TestWM);
  return runner.Run(shell_handle);
}
