// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_mus_unified.h"

#include <memory>
#include <utility>

#include "ash/host/ash_window_tree_host_mirroring_delegate.h"
#include "ash/host/root_window_transformer.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ash {

// TODO(msw): Fix targeting of events on the second display during capture.
class UnifiedEventTargeter : public aura::WindowTargeter {
 public:
  UnifiedEventTargeter(aura::Window* src_root,
                       aura::Window* dst_root,
                       AshWindowTreeHostMirroringDelegate* delegate)
      : src_root_(src_root), dst_root_(dst_root), delegate_(delegate) {
    DCHECK(delegate);
  }

  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    // Unlike the classic config, mus may provide a target for the event.
    if (root == src_root_) {
      delegate_->SetCurrentEventTargeterSourceHost(src_root_->GetHost());

      if (event->IsLocatedEvent()) {
        ui::LocatedEvent* located_event = static_cast<ui::LocatedEvent*>(event);
        located_event->ConvertLocationToTarget(
            static_cast<aura::Window*>(event->target()), dst_root_);
        located_event->set_root_location_f(located_event->location_f());
      }
      if (event->target())
        ui::Event::DispatcherApi(event).set_target(nullptr);
      ignore_result(
          dst_root_->GetHost()->event_sink()->OnEventFromSource(event));

      // Reset the source host.
      delegate_->SetCurrentEventTargeterSourceHost(nullptr);

      return nullptr;
    }

    NOTREACHED();
    return aura::WindowTargeter::FindTargetForEvent(root, event);
  }

 private:
  aura::Window* src_root_;
  aura::Window* dst_root_;
  AshWindowTreeHostMirroringDelegate* delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(UnifiedEventTargeter);
};

AshWindowTreeHostMusUnified::AshWindowTreeHostMusUnified(
    aura::WindowTreeHostMusInitParams init_params,
    AshWindowTreeHostMirroringDelegate* delegate)
    : AshWindowTreeHostMus(std::move(init_params)), delegate_(delegate) {
  DCHECK(delegate);
}

AshWindowTreeHostMusUnified::~AshWindowTreeHostMusUnified() {
  for (auto* ash_host : mirroring_hosts_)
    ash_host->AsWindowTreeHost()->window()->RemoveObserver(this);
}

void AshWindowTreeHostMusUnified::PrepareForShutdown() {
  AshWindowTreeHostMus::PrepareForShutdown();

  for (auto* host : mirroring_hosts_)
    host->PrepareForShutdown();
}

void AshWindowTreeHostMusUnified::RegisterMirroringHost(
    AshWindowTreeHost* mirroring_ash_host) {
  aura::Window* src_root = mirroring_ash_host->AsWindowTreeHost()->window();
  src_root->SetEventTargeter(
      std::make_unique<UnifiedEventTargeter>(src_root, window(), delegate_));
  DCHECK(std::find(mirroring_hosts_.begin(), mirroring_hosts_.end(),
                   mirroring_ash_host) == mirroring_hosts_.end());
  mirroring_hosts_.push_back(mirroring_ash_host);
  mirroring_ash_host->AsWindowTreeHost()->window()->AddObserver(this);
}

void AshWindowTreeHostMusUnified::SetBoundsInPixels(const gfx::Rect& bounds) {
  AshWindowTreeHostMus::SetBoundsInPixels(bounds);
  OnHostResizedInPixels(bounds.size());
}

void AshWindowTreeHostMusUnified::SetCursorNative(gfx::NativeCursor cursor) {
  for (auto* host : mirroring_hosts_)
    host->AsWindowTreeHost()->SetCursor(cursor);
}

void AshWindowTreeHostMusUnified::OnCursorVisibilityChangedNative(bool show) {
  for (auto* host : mirroring_hosts_)
    host->AsWindowTreeHost()->OnCursorVisibilityChanged(show);
}

void AshWindowTreeHostMusUnified::OnBoundsChanged(const gfx::Rect& bounds) {
  if (platform_window())
    OnHostResizedInPixels(bounds.size());
}

void AshWindowTreeHostMusUnified::OnWindowDestroying(aura::Window* window) {
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
