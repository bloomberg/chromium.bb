// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"

#if defined(USE_X11)
#include <X11/Xlib.h>

// Xlib.h defines RootWindow.
#undef RootWindow
#endif

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/display/root_window_transformers.h"
#include "ash/host/window_tree_host_factory.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/root_window_transformer.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
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

  if (!host_.get()) {
    const gfx::Rect& bounds_in_native = display_info.bounds_in_native();
    host_.reset(Shell::GetInstance()->window_tree_host_factory()->
        CreateWindowTreeHost(bounds_in_native));
    host_->window()->SetName(
        base::StringPrintf("MirrorRootWindow-%d", mirror_host_count++));
    host_->compositor()->SetBackgroundColor(SK_ColorBLACK);
    // No need to remove the observer because the DisplayController outlives the
    // host.
    host_->AddObserver(Shell::GetInstance()->display_controller());
    host_->AddObserver(this);
    // TODO(oshima): TouchHUD is using idkey.
    InitRootWindowSettings(host_->window())->display_id = display_info.id();
    host_->InitHost();
#if defined(USE_X11)
    DisableInput(host_->GetAcceleratedWidget());
#endif

    aura::client::SetCaptureClient(host_->window(), new NoneCaptureClient());
    host_->Show();

    // TODO(oshima): Start mirroring.
    aura::Window* mirror_window = new aura::Window(NULL);
    mirror_window->Init(aura::WINDOW_LAYER_TEXTURED);
    host_->window()->AddChild(mirror_window);
    mirror_window->SetBounds(host_->window()->bounds());
    mirror_window->Show();
    reflector_ = ui::ContextFactory::GetInstance()->CreateReflector(
        Shell::GetPrimaryRootWindow()->GetHost()->compositor(),
        mirror_window->layer());
  } else {
    GetRootWindowSettings(host_->window())->display_id = display_info.id();
    host_->SetBounds(display_info.bounds_in_native());
  }

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const DisplayInfo& source_display_info = display_manager->GetDisplayInfo(
      Shell::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(display_manager->IsMirrored());
  scoped_ptr<aura::RootWindowTransformer> transformer(
      CreateRootWindowTransformerForMirroredDisplay(source_display_info,
                                                    display_info));
  host_->SetRootWindowTransformer(transformer.Pass());
}

void MirrorWindowController::UpdateWindow() {
  if (host_.get()) {
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();
    const DisplayInfo& mirror_display_info = display_manager->GetDisplayInfo(
        display_manager->mirrored_display_id());
    UpdateWindow(mirror_display_info);
  }
}

void MirrorWindowController::Close() {
  if (host_.get()) {
    ui::ContextFactory::GetInstance()->RemoveReflector(reflector_);
    reflector_ = NULL;
    NoneCaptureClient* capture_client = static_cast<NoneCaptureClient*>(
        aura::client::GetCaptureClient(host_->window()));
    aura::client::SetCaptureClient(host_->window(), NULL);
    delete capture_client;

    host_->RemoveObserver(Shell::GetInstance()->display_controller());
    host_->RemoveObserver(this);
    host_.reset();
  }
}

void MirrorWindowController::OnHostResized(const aura::WindowTreeHost* host) {
  if (mirror_window_host_size_ == host->GetBounds().size())
    return;
  mirror_window_host_size_ = host->GetBounds().size();
  reflector_->OnMirroringCompositorResized();
  host_->SetRootWindowTransformer(CreateRootWindowTransformer().Pass());
  Shell::GetInstance()->display_controller()->cursor_window_controller()->
      UpdateLocation();
}


scoped_ptr<aura::RootWindowTransformer>
MirrorWindowController::CreateRootWindowTransformer() const {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const DisplayInfo& mirror_display_info = display_manager->GetDisplayInfo(
      display_manager->mirrored_display_id());
  const DisplayInfo& source_display_info = display_manager->GetDisplayInfo(
      Shell::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(display_manager->IsMirrored());
  return scoped_ptr<aura::RootWindowTransformer>(
      CreateRootWindowTransformerForMirroredDisplay(source_display_info,
                                                    mirror_display_info));
}

}  // namespace ash
