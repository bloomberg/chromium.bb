// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/viz/demo/host/demo_host.h"
#include "components/viz/demo/service/demo_service.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_WIN)
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/platform_window/win/win_window.h"
#endif

#if defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"  // nogncheck
#endif

namespace {

// Initializes and owns the components from base necessary to run the app.
class InitBase {
 public:
  InitBase(int argc, char** argv) {
    base::CommandLine::Init(argc, argv);
    base::i18n::InitializeICU();
    base::ThreadPoolInstance::CreateAndStartWithDefaultParams("demo");
  }

  ~InitBase() = default;

 private:
  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager_;
  base::SingleThreadTaskExecutor main_task_executor_{base::MessagePumpType::UI};

  DISALLOW_COPY_AND_ASSIGN(InitBase);
};

// Initializes and owns mojo.
class InitMojo {
 public:
  InitMojo() : thread_("Mojo thread") {
    mojo::core::Init();
    thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        thread_.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  }

  ~InitMojo() = default;

 private:
  base::Thread thread_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;

  DISALLOW_COPY_AND_ASSIGN(InitMojo);
};

// Initializes and owns the UI components needed for the app.
class InitUI {
 public:
  InitUI() {
#if defined(USE_X11)
    XInitThreads();
#endif
    event_source_ = ui::PlatformEventSource::CreateDefault();
  }

  ~InitUI() = default;

 private:
  std::unique_ptr<ui::PlatformEventSource> event_source_;

  DISALLOW_COPY_AND_ASSIGN(InitUI);
};

// DemoWindow creates the native window for the demo app. The native window
// provides a gfx::AcceleratedWidget, which is needed for the display
// compositor.
class DemoWindow : public ui::PlatformWindowDelegate {
 public:
  DemoWindow() = default;
  ~DemoWindow() override = default;

  void Create(const gfx::Rect& bounds) {
    platform_window_ = CreatePlatformWindow(bounds);
    platform_window_->Show();
    if (widget_ != gfx::kNullAcceleratedWidget)
      InitializeDemo();
  }

 private:
  std::unique_ptr<ui::PlatformWindow> CreatePlatformWindow(
      const gfx::Rect& bounds) {
    ui::PlatformWindowInitProperties props(bounds);
#if defined(USE_OZONE)
    return ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
        this, std::move(props));
#elif defined(OS_WIN)
    return std::make_unique<ui::WinWindow>(this, props.bounds);
#elif defined(USE_X11)
    auto x11_window = std::make_unique<ui::X11Window>(this);
    x11_window->Initialize(std::move(props));
    return x11_window;
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
  }

  void InitializeDemo() {
    DCHECK_NE(widget_, gfx::kNullAcceleratedWidget);
    // We finally have a valid gfx::AcceleratedWidget. We can now start the
    // actual process of setting up the viz host and the service.
    // First, set up the mojo message-pipes that the host and the service will
    // use to communicate with each other.
    mojo::PendingRemote<viz::mojom::FrameSinkManager> frame_sink_manager;
    mojo::PendingReceiver<viz::mojom::FrameSinkManager>
        frame_sink_manager_receiver =
            frame_sink_manager.InitWithNewPipeAndPassReceiver();
    mojo::PendingRemote<viz::mojom::FrameSinkManagerClient>
        frame_sink_manager_client;
    mojo::PendingReceiver<viz::mojom::FrameSinkManagerClient>
        frame_sink_manager_client_receiver =
            frame_sink_manager_client.InitWithNewPipeAndPassReceiver();

    // Next, create the host and the service, and pass them the right ends of
    // the message-pipes.
    host_ = std::make_unique<demo::DemoHost>(
        widget_, platform_window_->GetBounds().size(),
        std::move(frame_sink_manager_client_receiver),
        std::move(frame_sink_manager));

    service_ = std::make_unique<demo::DemoService>(
        std::move(frame_sink_manager_receiver),
        std::move(frame_sink_manager_client));
  }

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {
    host_->Resize(new_bounds.size());
  }

  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    widget_ = widget;
    if (platform_window_)
      InitializeDemo();
  }

  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}

  std::unique_ptr<demo::DemoHost> host_;
  std::unique_ptr<demo::DemoService> service_;

  std::unique_ptr<ui::PlatformWindow> platform_window_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindow);
};

int DemoMain() {
  DemoWindow window;
  window.Create(gfx::Rect(800, 600));

  base::RunLoop().Run();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  InitBase base(argc, argv);
  InitMojo mojo;
  InitUI ui;

  return DemoMain();
}
