// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_manager_aura.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromecast {

// An aura::WindowTreeHost that correctly converts input events.
class CastWindowTreeHost : public aura::WindowTreeHostPlatform {
 public:
  CastWindowTreeHost(bool enable_input, const gfx::Rect& bounds);
  ~CastWindowTreeHost() override;

  // aura::WindowTreeHostPlatform implementation:
  void DispatchEvent(ui::Event* event) override;

 private:
  const bool enable_input_;

  DISALLOW_COPY_AND_ASSIGN(CastWindowTreeHost);
};

CastWindowTreeHost::CastWindowTreeHost(bool enable_input,
                                       const gfx::Rect& bounds)
    : WindowTreeHostPlatform(bounds), enable_input_(enable_input) {}

CastWindowTreeHost::~CastWindowTreeHost() {}

void CastWindowTreeHost::DispatchEvent(ui::Event* event) {
  if (!enable_input_) {
    return;
  }

  if (event->IsKeyEvent()) {
    // Convert a RawKeyDown into a character insertion; otherwise
    // the WebContents will ignore most keyboard input.
    GetInputMethod()->DispatchKeyEvent(event->AsKeyEvent());
  } else {
    WindowTreeHostPlatform::DispatchEvent(event);
  }
}

// A layout manager owned by the root window.
class CastLayoutManager : public aura::LayoutManager {
 public:
  CastLayoutManager();
  ~CastLayoutManager() override;

 private:
  // aura::LayoutManager implementation:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  DISALLOW_COPY_AND_ASSIGN(CastLayoutManager);
};

CastLayoutManager::CastLayoutManager() {}
CastLayoutManager::~CastLayoutManager() {}

void CastLayoutManager::OnWindowResized() {
  // Invoked when the root window of the window tree host has been resized.
}

void CastLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  // Invoked when a window is added to the window tree host.
}

void CastLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  // Invoked when the root window of the window tree host will be removed.
}

void CastLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  // Invoked when the root window of the window tree host has been removed.
}

void CastLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visible) {
  // Note: this is invoked for child windows of the root window.
}

void CastLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

// static
std::unique_ptr<CastWindowManager> CastWindowManager::Create(
    bool enable_input) {
  return base::WrapUnique(new CastWindowManagerAura(enable_input));
}

CastWindowManagerAura::CastWindowManagerAura(bool enable_input)
    : enable_input_(enable_input) {}

CastWindowManagerAura::~CastWindowManagerAura() {
  TearDown();
}

void CastWindowManagerAura::Setup() {
  if (window_tree_host_) {
    return;
  }
  DCHECK(display::Screen::GetScreen());

  ui::InitializeInputMethodForTesting();

  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  LOG(INFO) << "Starting window manager, screen size: " << display_size.width()
            << "x" << display_size.height();
  CHECK(aura::Env::GetInstance());
  window_tree_host_.reset(
      new CastWindowTreeHost(enable_input_, gfx::Rect(display_size)));
  window_tree_host_->InitHost();
  window_tree_host_->window()->SetLayoutManager(new CastLayoutManager());

  // Allow seeing through to the hardware video plane:
  window_tree_host_->compositor()->SetHostHasTransparentBackground(true);
  window_tree_host_->compositor()->SetBackgroundColor(SK_ColorTRANSPARENT);

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(window_tree_host_->window()));

  CastVSyncSettings::GetInstance()->AddObserver(this);
  window_tree_host_->compositor()->SetAuthoritativeVSyncInterval(
      CastVSyncSettings::GetInstance()->GetVSyncInterval());

  window_tree_host_->Show();
}

void CastWindowManagerAura::TearDown() {
  if (!window_tree_host_) {
    return;
  }
  CastVSyncSettings::GetInstance()->RemoveObserver(this);
  capture_client_.reset();
  window_tree_host_.reset();
}

void CastWindowManagerAura::AddWindow(gfx::NativeView child) {
  LOG(INFO) << "Adding window: " << child->id() << ": " << child->GetName();
  Setup();

  DCHECK(child);
  aura::Window* parent = window_tree_host_->window();
  if (!parent->Contains(child)) {
    parent->AddChild(child);
  }

  parent->StackChildAtTop(child);
  child->SetBounds(window_tree_host_->window()->bounds());
}

void CastWindowManagerAura::OnVSyncIntervalChanged(base::TimeDelta interval) {
  DCHECK(window_tree_host_.get());
  window_tree_host_->compositor()->SetAuthoritativeVSyncInterval(interval);
}

}  // namespace chromecast
