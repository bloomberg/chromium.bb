// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_unified.h"

#include <utility>

#include "ash/host/root_window_transformer.h"
#include "ash/ime/input_method_event_handler.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ash {

class UnifiedEventTargeter : public aura::WindowTargeter {
 public:
  UnifiedEventTargeter(aura::Window* src_root, aura::Window* dst_root)
      : src_root_(src_root), dst_root_(dst_root) {}

  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    if (root == src_root_ && !event->target()) {
      if (event->IsLocatedEvent()) {
        ui::LocatedEvent* located_event = static_cast<ui::LocatedEvent*>(event);
        located_event->ConvertLocationToTarget(
            static_cast<aura::Window*>(nullptr), dst_root_);
      }
      ignore_result(
          dst_root_->GetHost()->event_processor()->OnEventFromSource(event));
      return nullptr;
    } else {
      NOTREACHED() << "event type:" << event->type();
      return aura::WindowTargeter::FindTargetForEvent(root, event);
    }
  }

  aura::Window* src_root_;
  aura::Window* dst_root_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedEventTargeter);
};

AshWindowTreeHostUnified::AshWindowTreeHostUnified(
    const gfx::Rect& initial_bounds)
    : AshWindowTreeHostPlatform() {
  scoped_ptr<ui::PlatformWindow> window(new ui::StubWindow(this));
  window->SetBounds(initial_bounds);
  SetPlatformWindow(std::move(window));
}

AshWindowTreeHostUnified::~AshWindowTreeHostUnified() {
  for (auto* ash_host : mirroring_hosts_)
    ash_host->AsWindowTreeHost()->window()->RemoveObserver(this);
}

void AshWindowTreeHostUnified::PrepareForShutdown() {
  AshWindowTreeHostPlatform::PrepareForShutdown();

  for (auto host : mirroring_hosts_)
    host->PrepareForShutdown();
}

void AshWindowTreeHostUnified::RegisterMirroringHost(
    AshWindowTreeHost* mirroring_ash_host) {
  aura::Window* src_root = mirroring_ash_host->AsWindowTreeHost()->window();
  src_root->SetEventTargeter(
      make_scoped_ptr(new UnifiedEventTargeter(src_root, window())));
  DCHECK(std::find(mirroring_hosts_.begin(), mirroring_hosts_.end(),
                   mirroring_ash_host) == mirroring_hosts_.end());
  mirroring_hosts_.push_back(mirroring_ash_host);
  mirroring_ash_host->AsWindowTreeHost()->window()->AddObserver(this);
}

void AshWindowTreeHostUnified::SetBounds(const gfx::Rect& bounds) {
  AshWindowTreeHostPlatform::SetBounds(bounds);
  OnHostResized(bounds.size());
}

void AshWindowTreeHostUnified::SetCursorNative(gfx::NativeCursor cursor) {
  for (auto host : mirroring_hosts_)
    host->AsWindowTreeHost()->SetCursor(cursor);
}

void AshWindowTreeHostUnified::OnCursorVisibilityChangedNative(bool show) {
  for (auto host : mirroring_hosts_)
    host->AsWindowTreeHost()->OnCursorVisibilityChanged(show);
}

void AshWindowTreeHostUnified::OnBoundsChanged(const gfx::Rect& bounds) {
  if (platform_window())
    OnHostResized(bounds.size());
}

void AshWindowTreeHostUnified::OnWindowDestroying(aura::Window* window) {
  auto iter =
      std::find_if(mirroring_hosts_.begin(), mirroring_hosts_.end(),
                   [window](AshWindowTreeHost* ash_host) {
                     return ash_host->AsWindowTreeHost()->window() == window;
                   });
  DCHECK(iter != mirroring_hosts_.end());
  window->RemoveObserver(this);
  mirroring_hosts_.erase(iter);
}

}  // namespace ash
