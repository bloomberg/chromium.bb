// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mojo_test_interface_factory.h"

#include <memory>
#include <utility>

#include "ash/metrics/time_to_first_present_recorder_test_api.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/interfaces/shelf_test_api.mojom.h"
#include "ash/public/interfaces/shell_test_api.mojom.h"
#include "ash/public/interfaces/status_area_widget_test_api.mojom.h"
#include "ash/public/interfaces/system_tray_test_api.mojom.h"
#include "ash/public/interfaces/time_to_first_present_recorder_test_api.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_test_api.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/shell_test_api.h"
#include "ash/system/status_area_widget_test_api.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/system/unified/unified_system_tray_test_api.h"
#include "ash/ws/window_service_owner.h"
#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "services/ui/public/interfaces/test_event_injector.mojom.h"
#include "services/ui/ws2/test_event_injector.h"
#include "services/ui/ws2/test_event_injector_delegate.h"

namespace ash {
namespace mojo_test_interface_factory {
namespace {

class TestEventInjectorHelper;

TestEventInjectorHelper* g_test_event_injector_helper;

// Helper class to create TestInjector. Also acts as the delegate to lookup
// the WindowTreeHost for a display-id. This class destroys itself when the
// Shell is destroyed.
class TestEventInjectorHelper : public ShellObserver,
                                public ui::ws2::TestEventInjectorDelegate {
 public:
  // Gets the single instance, creating if necessary.
  static TestEventInjectorHelper* Get() {
    if (!g_test_event_injector_helper)
      g_test_event_injector_helper = new TestEventInjectorHelper();
    return g_test_event_injector_helper;
  }

  void AddBinding(ui::mojom::TestEventInjectorRequest request) {
    test_event_injector_->AddBinding(std::move(request));
  }

  // ShellObserver:
  void OnShellDestroyed() override { delete this; }

  // ui::ws2::TestEventInjectorDelegate:
  aura::WindowTreeHost* GetWindowTreeHostForDisplayId(
      int64_t display_id) override {
    RootWindowController* root_window_controller =
        Shell::GetRootWindowControllerWithDisplayId(display_id);
    return root_window_controller ? root_window_controller->GetHost() : nullptr;
  }

 private:
  TestEventInjectorHelper() {
    DCHECK(!g_test_event_injector_helper);
    g_test_event_injector_helper = this;
    Shell::Get()->AddShellObserver(this);
    test_event_injector_ = std::make_unique<ui::ws2::TestEventInjector>(
        Shell::Get()->window_service_owner()->window_service(), this);
  }

  ~TestEventInjectorHelper() override {
    Shell::Get()->RemoveShellObserver(this);
    DCHECK_EQ(this, g_test_event_injector_helper);
    g_test_event_injector_helper = nullptr;
  }

  std::unique_ptr<ui::ws2::TestEventInjector> test_event_injector_;

  DISALLOW_COPY_AND_ASSIGN(TestEventInjectorHelper);
};

// These functions aren't strictly necessary, but exist to make threading and
// arguments clearer.

void BindShelfTestApiOnMainThread(mojom::ShelfTestApiRequest request) {
  ShelfTestApi::BindRequest(std::move(request));
}

void BindShellTestApiOnMainThread(mojom::ShellTestApiRequest request) {
  ShellTestApi::BindRequest(std::move(request));
}

void BindStatusAreaWidgetTestApiOnMainThread(
    mojom::StatusAreaWidgetTestApiRequest request) {
  StatusAreaWidgetTestApi::BindRequest(std::move(request));
}

void BindSystemTrayTestApiOnMainThread(
    mojom::SystemTrayTestApiRequest request) {
  if (features::IsSystemTrayUnifiedEnabled())
    UnifiedSystemTrayTestApi::BindRequest(std::move(request));
  else
    SystemTrayTestApi::BindRequest(std::move(request));
}

void BindTestEventInjectorOnMainThread(
    ui::mojom::TestEventInjectorRequest request) {
  TestEventInjectorHelper::Get()->AddBinding(std::move(request));
}

void BindTimeToFirstPresentRecorderTestApiOnMainThread(
    mojom::TimeToFirstPresentRecorderTestApiRequest request) {
  TimeToFirstPresentRecorderTestApi::BindRequest(std::move(request));
}

}  // namespace

void RegisterInterfaces(
    service_manager::BinderRegistry* registry,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  registry->AddInterface(base::Bind(&BindShelfTestApiOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindShellTestApiOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindStatusAreaWidgetTestApiOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindSystemTrayTestApiOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(
      base::Bind(&BindTimeToFirstPresentRecorderTestApiOnMainThread),
      main_thread_task_runner);
}

void RegisterWindowServiceInterfaces(
    service_manager::BinderRegistry* registry) {
  registry->AddInterface(base::Bind(&BindTestEventInjectorOnMainThread));
}

}  // namespace mojo_test_interface_factory
}  // namespace ash
