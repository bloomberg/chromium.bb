// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_port_classic.h"

#include <memory>
#include <utility>

#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/pointer_watcher_adapter_classic.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/env.h"

namespace ash {

ShellPortClassic::ShellPortClassic() = default;

ShellPortClassic::~ShellPortClassic() = default;

// static
ShellPortClassic* ShellPortClassic::Get() {
  CHECK(Shell::GetAshConfig() == Config::CLASSIC);
  return static_cast<ShellPortClassic*>(ShellPort::Get());
}

void ShellPortClassic::Shutdown() {
  pointer_watcher_adapter_.reset();

  ShellPort::Shutdown();
}

Config ShellPortClassic::GetAshConfig() const {
  return Config::CLASSIC;
}

void ShellPortClassic::AddPointerWatcher(
    views::PointerWatcher* watcher,
    views::PointerWatcherEventTypes events) {
  pointer_watcher_adapter_->AddPointerWatcher(watcher, events);
}

void ShellPortClassic::RemovePointerWatcher(views::PointerWatcher* watcher) {
  pointer_watcher_adapter_->RemovePointerWatcher(watcher);
}

bool ShellPortClassic::IsTouchDown() {
  return aura::Env::GetInstance()->is_touch_down();
}

void ShellPortClassic::ToggleIgnoreExternalKeyboard() {
  Shell::Get()->virtual_keyboard_controller()->ToggleIgnoreExternalKeyboard();
}

void ShellPortClassic::CreatePointerWatcherAdapter() {
  pointer_watcher_adapter_ = std::make_unique<PointerWatcherAdapterClassic>();
}

void ShellPortClassic::OnCreatedRootWindowContainers(
    RootWindowController* root_window_controller) {}

void ShellPortClassic::UpdateSystemModalAndBlockingContainers() {}

void ShellPortClassic::OnHostsInitialized() {}

void ShellPortClassic::AddVideoDetectorObserver(
    viz::mojom::VideoDetectorObserverPtr observer) {
  aura::Env::GetInstance()
      ->context_factory_private()
      ->GetHostFrameSinkManager()
      ->AddVideoDetectorObserver(std::move(observer));
}

}  // namespace ash
