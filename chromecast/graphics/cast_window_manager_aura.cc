// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_manager_aura.h"

#include "base/memory/ptr_util.h"
#include "chromecast/graphics/cast_focus_client_aura.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromecast {

// An ui::EventTarget that ignores events.
class CastEventIgnorer : public ui::EventTargeter {
 public:
  ~CastEventIgnorer() override;

  // ui::EventTargeter implementation:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override;
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override;
};

CastEventIgnorer::~CastEventIgnorer() {}

ui::EventTarget* CastEventIgnorer::FindTargetForEvent(ui::EventTarget* root,
                                                      ui::Event* event) {
  return nullptr;
}

ui::EventTarget* CastEventIgnorer::FindNextBestTarget(
    ui::EventTarget* previous_target,
    ui::Event* event) {
  return nullptr;
}

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
    : WindowTreeHostPlatform(bounds), enable_input_(enable_input) {
  if (!enable_input) {
    window()->SetEventTargeter(
        std::unique_ptr<ui::EventTargeter>(new CastEventIgnorer));
  }
}

CastWindowTreeHost::~CastWindowTreeHost() {}

void CastWindowTreeHost::DispatchEvent(ui::Event* event) {
  if (!enable_input_) {
    return;
  }

  WindowTreeHostPlatform::DispatchEvent(event);
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
  aura::Window* parent = child->parent();
  if (!visible || !parent->IsRootWindow()) {
    return;
  }

  // We set z-order here, because the window's ID could be set after the window
  // is added to the layout, particularly when using views::Widget to create the
  // window.  The window ID must be set prior to showing the window.  Since we
  // only process z-order on visibility changes for top-level windows, this
  // logic will execute infrequently.

  // Determine z-order relative to existing windows.
  aura::Window::Windows windows = parent->children();
  aura::Window* above = nullptr;
  aura::Window* below = nullptr;
  for (auto* other : windows) {
    if (other == child) {
      continue;
    }
    if ((other->id() < child->id()) && (!below || other->id() > below->id())) {
      below = other;
    } else if ((other->id() > child->id()) &&
               (!above || other->id() < above->id())) {
      above = other;
    }
  }

  // Adjust the z-order of the new child window.
  if (above) {
    parent->StackChildBelow(child, above);
  } else if (below) {
    parent->StackChildAbove(child, below);
  } else {
    parent->StackChildAtBottom(child);
  }
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
  window_tree_host_->compositor()->SetBackgroundColor(SK_ColorTRANSPARENT);

  focus_client_.reset(new CastFocusClientAura());
  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());
  wm::SetActivationClient(window_tree_host_->window(), focus_client_.get());
  aura::client::SetWindowParentingClient(window_tree_host_->window(), this);
  capture_client_.reset(
      new aura::client::DefaultCaptureClient(window_tree_host_->window()));

  window_tree_host_->Show();
}

void CastWindowManagerAura::TearDown() {
  if (!window_tree_host_) {
    return;
  }
  capture_client_.reset();
  aura::client::SetWindowParentingClient(window_tree_host_->window(), nullptr);
  wm::SetActivationClient(window_tree_host_->window(), nullptr);
  aura::client::SetFocusClient(window_tree_host_->window(), nullptr);
  focus_client_.reset();
  window_tree_host_.reset();
}

void CastWindowManagerAura::SetWindowId(gfx::NativeView window,
                                        WindowId window_id) {
  window->set_id(window_id);
}

void CastWindowManagerAura::InjectEvent(ui::Event* event) {
  if (!window_tree_host_) {
    return;
  }
  window_tree_host_->DispatchEvent(event);
}

gfx::NativeView CastWindowManagerAura::GetRootWindow() {
  Setup();
  return window_tree_host_->window();
}

aura::Window* CastWindowManagerAura::GetDefaultParent(aura::Window* window,
                                                      const gfx::Rect& bounds) {
  DCHECK(window_tree_host_);
  return window_tree_host_->window();
}

void CastWindowManagerAura::AddWindow(gfx::NativeView child) {
  LOG(INFO) << "Adding window: " << child->id() << ": " << child->GetName();
  Setup();

  DCHECK(child);
  aura::Window* parent = window_tree_host_->window();
  if (!parent->Contains(child)) {
    parent->AddChild(child);
  }
}

}  // namespace chromecast
