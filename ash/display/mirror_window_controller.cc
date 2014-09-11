// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

// Xlib.h defines RootWindow.
#undef RootWindow
#endif

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/display/root_window_transformers.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/compositor/reflector.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#endif

namespace ash {
namespace {

#if defined(USE_X11)
// Mirror window shouldn't handle input events.
void DisableInput(XID window) {
  long event_mask = ExposureMask | VisibilityChangeMask |
      StructureNotifyMask | PropertyChangeMask;
  XSelectInput(gfx::GetXDisplay(), window, event_mask);
  unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {0};
  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(gfx::GetXDisplay(), window, &evmask, 1);
}
#endif

class NoneCaptureClient : public aura::client::CaptureClient {
 public:
  NoneCaptureClient() {}
  virtual ~NoneCaptureClient() {}

 private:
  // Does a capture on the |window|.
  virtual void SetCapture(aura::Window* window) OVERRIDE {}

  // Releases a capture from the |window|.
  virtual void ReleaseCapture(aura::Window* window) OVERRIDE {}

  // Returns the current capture window.
  virtual aura::Window* GetCaptureWindow() OVERRIDE {
    return NULL;
  }
  virtual aura::Window* GetGlobalCaptureWindow() OVERRIDE {
    return NULL;
  }

  DISALLOW_COPY_AND_ASSIGN(NoneCaptureClient);
};

}  // namespace

MirrorWindowController::MirrorWindowController() {}

MirrorWindowController::~MirrorWindowController() {
  // Make sure the root window gets deleted before cursor_window_delegate.
  Close();
}

void MirrorWindowController::UpdateWindow(const DisplayInfo& display_info) {
  static int mirror_host_count = 0;
  if (!ash_host_.get()) {
    AshWindowTreeHostInitParams init_params;
    init_params.initial_bounds = display_info.bounds_in_native();
    ash_host_.reset(AshWindowTreeHost::Create(init_params));
    aura::WindowTreeHost* host = ash_host_->AsWindowTreeHost();
    host->window()->SetName(
        base::StringPrintf("MirrorRootWindow-%d", mirror_host_count++));
    host->compositor()->SetBackgroundColor(SK_ColorBLACK);
    // No need to remove the observer because the DisplayController outlives the
    // host.
    host->AddObserver(Shell::GetInstance()->display_controller());
    host->AddObserver(this);
    // TODO(oshima): TouchHUD is using idkey.
    InitRootWindowSettings(host->window())->display_id = display_info.id();
    host->InitHost();
#if defined(USE_X11)
    DisableInput(host->GetAcceleratedWidget());
#endif

    aura::client::SetCaptureClient(host->window(), new NoneCaptureClient());
    host->Show();

    // TODO(oshima): Start mirroring.
    aura::Window* mirror_window = new aura::Window(NULL);
    mirror_window->Init(aura::WINDOW_LAYER_TEXTURED);
    host->window()->AddChild(mirror_window);
    mirror_window->SetBounds(host->window()->bounds());
    mirror_window->Show();
    reflector_ = aura::Env::GetInstance()->context_factory()->CreateReflector(
        Shell::GetPrimaryRootWindow()->GetHost()->compositor(),
        mirror_window->layer());
  } else {
    aura::WindowTreeHost* host = ash_host_->AsWindowTreeHost();
    GetRootWindowSettings(host->window())->display_id = display_info.id();
    host->SetBounds(display_info.bounds_in_native());
  }

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const DisplayInfo& source_display_info = display_manager->GetDisplayInfo(
      Shell::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(display_manager->IsMirrored());
  scoped_ptr<RootWindowTransformer> transformer(
      CreateRootWindowTransformerForMirroredDisplay(source_display_info,
                                                    display_info));
  ash_host_->SetRootWindowTransformer(transformer.Pass());
}

void MirrorWindowController::UpdateWindow() {
  if (ash_host_.get()) {
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();
    const DisplayInfo& mirror_display_info = display_manager->GetDisplayInfo(
        display_manager->mirrored_display_id());
    UpdateWindow(mirror_display_info);
  }
}

void MirrorWindowController::Close() {
  if (ash_host_.get()) {
    aura::WindowTreeHost* host = ash_host_->AsWindowTreeHost();
    aura::Env::GetInstance()->context_factory()->RemoveReflector(reflector_);
    reflector_ = NULL;
    NoneCaptureClient* capture_client = static_cast<NoneCaptureClient*>(
        aura::client::GetCaptureClient(host->window()));
    aura::client::SetCaptureClient(host->window(), NULL);
    delete capture_client;

    host->RemoveObserver(Shell::GetInstance()->display_controller());
    host->RemoveObserver(this);
    ash_host_.reset();
  }
}

void MirrorWindowController::OnHostResized(const aura::WindowTreeHost* host) {
  if (mirror_window_host_size_ == host->GetBounds().size())
    return;
  mirror_window_host_size_ = host->GetBounds().size();
  reflector_->OnMirroringCompositorResized();
  ash_host_->SetRootWindowTransformer(CreateRootWindowTransformer().Pass());
  Shell::GetInstance()->display_controller()->cursor_window_controller()->
      UpdateLocation();
}

aura::Window* MirrorWindowController::GetWindow() {
  return ash_host_.get() ? ash_host_->AsWindowTreeHost()->window() : NULL;
}

scoped_ptr<RootWindowTransformer>
MirrorWindowController::CreateRootWindowTransformer() const {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const DisplayInfo& mirror_display_info = display_manager->GetDisplayInfo(
      display_manager->mirrored_display_id());
  const DisplayInfo& source_display_info = display_manager->GetDisplayInfo(
      Shell::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(display_manager->IsMirrored());
  return scoped_ptr<RootWindowTransformer>(
      CreateRootWindowTransformerForMirroredDisplay(source_display_info,
                                                    mirror_display_info));
}

}  // namespace ash
