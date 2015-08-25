// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_unified.h"
#include "ash/host/root_window_transformer.h"
#include "ash/ime/input_method_event_handler.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event_processor.h"
#include "ui/events/null_event_targeter.h"
#include "ui/gfx/geometry/insets.h"

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
    : bounds_(gfx::Rect(initial_bounds.size())), transformer_helper_(this) {
  CreateCompositor();
  OnAcceleratedWidgetAvailable();
  transformer_helper_.Init();
}

AshWindowTreeHostUnified::~AshWindowTreeHostUnified() {
  for (auto* ash_host : mirroring_hosts_)
    ash_host->AsWindowTreeHost()->window()->RemoveObserver(this);

  DestroyCompositor();
  DestroyDispatcher();
}

void AshWindowTreeHostUnified::PrepareForShutdown() {
  window()->SetEventTargeter(
      scoped_ptr<ui::EventTargeter>(new ui::NullEventTargeter));

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

void AshWindowTreeHostUnified::ToggleFullScreen() {
}

bool AshWindowTreeHostUnified::ConfineCursorToRootWindow() {
  return true;
}

void AshWindowTreeHostUnified::UnConfineCursor() {
}

void AshWindowTreeHostUnified::SetRootWindowTransformer(
    scoped_ptr<RootWindowTransformer> transformer) {
  transformer_helper_.SetRootWindowTransformer(transformer.Pass());
}

gfx::Insets AshWindowTreeHostUnified::GetHostInsets() const {
  return transformer_helper_.GetHostInsets();
}

aura::WindowTreeHost* AshWindowTreeHostUnified::AsWindowTreeHost() {
  return this;
}

ui::EventSource* AshWindowTreeHostUnified::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget AshWindowTreeHostUnified::GetAcceleratedWidget() {
  return gfx::kNullAcceleratedWidget;
}

void AshWindowTreeHostUnified::ShowImpl() {
}

void AshWindowTreeHostUnified::HideImpl() {
}

gfx::Rect AshWindowTreeHostUnified::GetBounds() const {
  return bounds_;
}

void AshWindowTreeHostUnified::SetBounds(const gfx::Rect& bounds) {
  bounds_.set_size(bounds.size());
  OnHostResized(bounds_.size());
}

gfx::Transform AshWindowTreeHostUnified::GetRootTransform() const {
  return transformer_helper_.GetTransform();
}

void AshWindowTreeHostUnified::SetRootTransform(
    const gfx::Transform& transform) {
  transformer_helper_.SetTransform(transform);
}

gfx::Transform AshWindowTreeHostUnified::GetInverseRootTransform() const {
  return transformer_helper_.GetInverseTransform();
}

void AshWindowTreeHostUnified::UpdateRootWindowSize(
    const gfx::Size& host_size) {
  transformer_helper_.UpdateWindowSize(host_size);
}

void AshWindowTreeHostUnified::SetCapture() {
}

void AshWindowTreeHostUnified::ReleaseCapture() {
}

gfx::Point AshWindowTreeHostUnified::GetLocationOnNativeScreen() const {
  return gfx::Point();
}

void AshWindowTreeHostUnified::SetCursorNative(gfx::NativeCursor cursor) {
  for (auto host : mirroring_hosts_)
    host->AsWindowTreeHost()->SetCursor(cursor);
}

void AshWindowTreeHostUnified::MoveCursorToNative(const gfx::Point& location) {
  // No native cursor in offscreen surface.
}

void AshWindowTreeHostUnified::OnCursorVisibilityChangedNative(bool show) {
  for (auto host : mirroring_hosts_)
    host->AsWindowTreeHost()->OnCursorVisibilityChanged(show);
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

ui::EventDispatchDetails AshWindowTreeHostUnified::DispatchKeyEventPostIME(
    ui::KeyEvent* event) {
  input_method_handler()->SetPostIME(true);
  ui::EventDispatchDetails details =
      event_processor()->OnEventFromSource(event);
  if (!details.dispatcher_destroyed)
    input_method_handler()->SetPostIME(false);
  return details;
}

}  // namespace ash
