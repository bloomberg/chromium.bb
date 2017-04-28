// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"

#include <utility>

#if defined(USE_X11)
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

// Xlib.h defines RootWindow.
#undef RootWindow
#endif

#include "ash/display/cursor_window_controller.h"
#include "ash/display/root_window_transformers.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/compositor/reflector.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_X11)
#include "ui/aura/window_tree_host_x11.h"
#include "ui/gfx/x/x11_types.h"  // nogncheck
#endif

namespace ash {
namespace {

// ScreenPositionClient for mirroring windows.
class MirroringScreenPositionClient
    : public aura::client::ScreenPositionClient {
 public:
  explicit MirroringScreenPositionClient(MirrorWindowController* controller)
      : controller_(controller) {}

  void ConvertPointToScreen(const aura::Window* window,
                            gfx::Point* point) override {
    const aura::Window* root = window->GetRootWindow();
    aura::Window::ConvertPointToTarget(window, root, point);
    const display::Display& display =
        controller_->GetDisplayForRootWindow(root);
    const gfx::Point display_origin = display.bounds().origin();
    point->Offset(display_origin.x(), display_origin.y());
  }

  void ConvertPointFromScreen(const aura::Window* window,
                              gfx::Point* point) override {
    const aura::Window* root = window->GetRootWindow();
    const display::Display& display =
        controller_->GetDisplayForRootWindow(root);
    const gfx::Point display_origin = display.bounds().origin();
    point->Offset(-display_origin.x(), -display_origin.y());
    aura::Window::ConvertPointToTarget(root, window, point);
  }

  void ConvertHostPointToScreen(aura::Window* root_window,
                                gfx::Point* point) override {
    aura::Window* not_used;
    ScreenPositionController::ConvertHostPointToRelativeToRootWindow(
        root_window, controller_->GetAllRootWindows(), point, &not_used);
    ConvertPointToScreen(root_window, point);
  }

  void SetBounds(aura::Window* window,
                 const gfx::Rect& bounds,
                 const display::Display& display) override {
    NOTREACHED();
  }

 private:
  MirrorWindowController* controller_;  // not owned.

  DISALLOW_COPY_AND_ASSIGN(MirroringScreenPositionClient);
};

// A trivial CaptureClient that does nothing. That is, calls to set/release
// capture are dropped.
class NoneCaptureClient : public aura::client::CaptureClient {
 public:
  NoneCaptureClient() {}
  ~NoneCaptureClient() override {}

 private:
  // aura::client::CaptureClient:
  void SetCapture(aura::Window* window) override {}
  void ReleaseCapture(aura::Window* window) override {}
  aura::Window* GetCaptureWindow() override { return nullptr; }
  aura::Window* GetGlobalCaptureWindow() override { return nullptr; }
  void AddObserver(aura::client::CaptureClientObserver* observer) override {}
  void RemoveObserver(aura::client::CaptureClientObserver* observer) override {}

  DISALLOW_COPY_AND_ASSIGN(NoneCaptureClient);
};

display::DisplayManager::MultiDisplayMode GetCurrentMultiDisplayMode() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  return display_manager->IsInUnifiedMode()
             ? display::DisplayManager::UNIFIED
             : (display_manager->IsInMirrorMode()
                    ? display::DisplayManager::MIRRORING
                    : display::DisplayManager::EXTENDED);
}

}  // namespace

struct MirrorWindowController::MirroringHostInfo {
  MirroringHostInfo();
  ~MirroringHostInfo();
  std::unique_ptr<AshWindowTreeHost> ash_host;
  gfx::Size mirror_window_host_size;
  aura::Window* mirror_window = nullptr;
};

MirrorWindowController::MirroringHostInfo::MirroringHostInfo() {}
MirrorWindowController::MirroringHostInfo::~MirroringHostInfo() {}

MirrorWindowController::MirrorWindowController()
    : multi_display_mode_(display::DisplayManager::EXTENDED),
      screen_position_client_(new MirroringScreenPositionClient(this)) {}

MirrorWindowController::~MirrorWindowController() {
  // Make sure the root window gets deleted before cursor_window_delegate.
  Close(false);
}

void MirrorWindowController::UpdateWindow(
    const std::vector<display::ManagedDisplayInfo>& display_info_list) {
  static int mirror_host_count = 0;
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const display::Display& primary =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  const display::ManagedDisplayInfo& source_display_info =
      display_manager->GetDisplayInfo(primary.id());

  multi_display_mode_ = GetCurrentMultiDisplayMode();

  for (const display::ManagedDisplayInfo& display_info : display_info_list) {
    std::unique_ptr<RootWindowTransformer> transformer;
    if (display_manager->IsInMirrorMode()) {
      transformer.reset(CreateRootWindowTransformerForMirroredDisplay(
          source_display_info, display_info));
    } else if (display_manager->IsInUnifiedMode()) {
      display::Display display =
          display_manager->GetMirroringDisplayById(display_info.id());
      transformer.reset(CreateRootWindowTransformerForUnifiedDesktop(
          primary.bounds(), display));
    } else {
      NOTREACHED();
    }

    if (mirroring_host_info_map_.find(display_info.id()) ==
        mirroring_host_info_map_.end()) {
      AshWindowTreeHostInitParams init_params;
      init_params.initial_bounds = display_info.bounds_in_native();
      init_params.display_id = display_info.id();
      init_params.device_scale_factor = display_info.device_scale_factor();
      init_params.ui_scale_factor = display_info.configured_ui_scale();
      MirroringHostInfo* host_info = new MirroringHostInfo;
      host_info->ash_host = AshWindowTreeHost::Create(init_params);
      mirroring_host_info_map_[display_info.id()] = host_info;

      aura::WindowTreeHost* host = host_info->ash_host->AsWindowTreeHost();
      // TODO: Config::MUS should not install an InputMethod.
      // http://crbug.com/706913
      if (!host->has_input_method()) {
        host->SetSharedInputMethod(
            Shell::Get()->window_tree_host_manager()->input_method());
      }
      host->window()->SetName(
          base::StringPrintf("MirrorRootWindow-%d", mirror_host_count++));
      host->compositor()->SetBackgroundColor(SK_ColorBLACK);
      // No need to remove the observer because the WindowTreeHostManager
      // outlives the host.
      host->AddObserver(Shell::Get()->window_tree_host_manager());
      host->AddObserver(this);
      // TODO(oshima): TouchHUD is using idkey.
      InitRootWindowSettings(host->window())->display_id = display_info.id();
      host->InitHost();
      host->window()->Show();
#if defined(USE_X11)
      if (!display_manager->IsInUnifiedMode()) {
        // Mirror window shouldn't handle input events.
        static_cast<aura::WindowTreeHostX11*>(host)->DisableInput();
      }
#endif

      if (display_manager->IsInUnifiedMode()) {
        host_info->ash_host->ConfineCursorToRootWindow();
        AshWindowTreeHost* unified_ash_host =
            Shell::Get()
                ->window_tree_host_manager()
                ->GetAshWindowTreeHostForDisplayId(
                    display::Screen::GetScreen()->GetPrimaryDisplay().id());
        unified_ash_host->RegisterMirroringHost(host_info->ash_host.get());
        aura::client::SetScreenPositionClient(host->window(),
                                              screen_position_client_.get());
      }

      aura::client::SetCaptureClient(host->window(), new NoneCaptureClient());
      host->Show();

      aura::Window* mirror_window = host_info->mirror_window =
          new aura::Window(nullptr);
      mirror_window->Init(ui::LAYER_SOLID_COLOR);
      host->window()->AddChild(mirror_window);
      host_info->ash_host->SetRootWindowTransformer(std::move(transformer));
      mirror_window->SetBounds(host->window()->bounds());
      mirror_window->Show();
      if (reflector_) {
        reflector_->AddMirroringLayer(mirror_window->layer());
      } else if (aura::Env::GetInstance()->context_factory_private()) {
        reflector_ =
            aura::Env::GetInstance()
                ->context_factory_private()
                ->CreateReflector(
                    Shell::GetPrimaryRootWindow()->GetHost()->compositor(),
                    mirror_window->layer());
      } else {
        // TODO: Config::MUS needs to support reflector.
        // http://crbug.com/601869.
        NOTIMPLEMENTED();
      }
    } else {
      AshWindowTreeHost* ash_host =
          mirroring_host_info_map_[display_info.id()]->ash_host.get();
      aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();
      GetRootWindowSettings(host->window())->display_id = display_info.id();
      ash_host->SetRootWindowTransformer(std::move(transformer));
      host->SetBoundsInPixels(display_info.bounds_in_native());
    }
  }

  // Deleting WTHs for disconnected displays.
  if (mirroring_host_info_map_.size() > display_info_list.size()) {
    for (MirroringHostInfoMap::iterator iter = mirroring_host_info_map_.begin();
         iter != mirroring_host_info_map_.end();) {
      if (std::find_if(display_info_list.begin(), display_info_list.end(),
                       [iter](const display::ManagedDisplayInfo& info) {
                         return info.id() == iter->first;
                       }) == display_info_list.end()) {
        CloseAndDeleteHost(iter->second, true);
        iter = mirroring_host_info_map_.erase(iter);
      } else {
        ++iter;
      }
    }
  }
}

void MirrorWindowController::UpdateWindow() {
  if (mirroring_host_info_map_.empty())
    return;
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  std::vector<display::ManagedDisplayInfo> display_info_list;
  for (auto& pair : mirroring_host_info_map_)
    display_info_list.push_back(display_manager->GetDisplayInfo(pair.first));
  UpdateWindow(display_info_list);
}

void MirrorWindowController::CloseIfNotNecessary() {
  display::DisplayManager::MultiDisplayMode new_mode =
      GetCurrentMultiDisplayMode();
  if (multi_display_mode_ != new_mode)
    Close(true);
  multi_display_mode_ = new_mode;
}

void MirrorWindowController::Close(bool delay_host_deletion) {
  for (auto& info : mirroring_host_info_map_)
    CloseAndDeleteHost(info.second, delay_host_deletion);

  mirroring_host_info_map_.clear();
  if (reflector_) {
    aura::Env::GetInstance()->context_factory_private()->RemoveReflector(
        reflector_.get());
    reflector_.reset();
  }
}

void MirrorWindowController::OnHostResized(const aura::WindowTreeHost* host) {
  for (auto& pair : mirroring_host_info_map_) {
    MirroringHostInfo* info = pair.second;
    if (info->ash_host->AsWindowTreeHost() == host) {
      if (info->mirror_window_host_size == host->GetBoundsInPixels().size())
        return;
      info->mirror_window_host_size = host->GetBoundsInPixels().size();
      // TODO: |reflector_| should always be non-null here, but isn't in MUS
      // yet because of http://crbug.com/601869.
      if (reflector_)
        reflector_->OnMirroringCompositorResized();
      // No need to update the transformer as new transformer is already set
      // in UpdateWindow.
      Shell::Get()
          ->window_tree_host_manager()
          ->cursor_window_controller()
          ->UpdateLocation();
      return;
    }
  }
}

aura::Window* MirrorWindowController::GetWindow() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  if (!display_manager->IsInMirrorMode() || mirroring_host_info_map_.empty())
    return nullptr;
  DCHECK_EQ(1U, mirroring_host_info_map_.size());
  return mirroring_host_info_map_.begin()
      ->second->ash_host->AsWindowTreeHost()
      ->window();
}

display::Display MirrorWindowController::GetDisplayForRootWindow(
    const aura::Window* root) const {
  for (const auto& pair : mirroring_host_info_map_) {
    if (pair.second->ash_host->AsWindowTreeHost()->window() == root) {
      // Sanity check to catch an error early.
      int64_t id = pair.first;
      const display::Displays& list =
          Shell::Get()->display_manager()->software_mirroring_display_list();
      auto iter = std::find_if(
          list.begin(), list.end(),
          [id](const display::Display& display) { return display.id() == id; });
      DCHECK(iter != list.end());
      if (iter != list.end())
        return *iter;
    }
  }
  return display::Display();
}

AshWindowTreeHost* MirrorWindowController::GetAshWindowTreeHostForDisplayId(
    int64_t id) {
  CHECK_EQ(1u, mirroring_host_info_map_.count(id));
  return mirroring_host_info_map_[id]->ash_host.get();
}

aura::Window::Windows MirrorWindowController::GetAllRootWindows() const {
  aura::Window::Windows root_windows;
  for (const auto& pair : mirroring_host_info_map_)
    root_windows.push_back(pair.second->ash_host->AsWindowTreeHost()->window());
  return root_windows;
}

void MirrorWindowController::CloseAndDeleteHost(MirroringHostInfo* host_info,
                                                bool delay_host_deletion) {
  aura::WindowTreeHost* host = host_info->ash_host->AsWindowTreeHost();

  aura::client::SetScreenPositionClient(host->window(), nullptr);

  NoneCaptureClient* capture_client = static_cast<NoneCaptureClient*>(
      aura::client::GetCaptureClient(host->window()));
  aura::client::SetCaptureClient(host->window(), nullptr);
  delete capture_client;

  host->RemoveObserver(Shell::Get()->window_tree_host_manager());
  host->RemoveObserver(this);
  host_info->ash_host->PrepareForShutdown();
  // TODO: |reflector_| should always be non-null here, but isn't in MUS yet
  // because of http://crbug.com/601869.
  if (reflector_)
    reflector_->RemoveMirroringLayer(host_info->mirror_window->layer());

  // EventProcessor may be accessed after this call if the mirroring window
  // was deleted as a result of input event (e.g. shortcut), so don't delete
  // now.
  if (delay_host_deletion)
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, host_info);
  else
    delete host_info;
}

}  // namespace ash
