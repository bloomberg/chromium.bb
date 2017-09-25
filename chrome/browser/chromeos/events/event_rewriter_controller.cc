// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/event_rewriter_controller.h"

#include <utility>

#include "ash/display/mirror_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"

namespace chromeos {

EventRewriterController::EventRewriterController() : initialized_(false) {
  // Add the controller as an observer for new root windows.
  aura::Env::GetInstance()->AddObserver(this);
}

EventRewriterController::~EventRewriterController() {
  aura::Env::GetInstance()->RemoveObserver(this);
  // Remove the rewriters from every root window EventSource and destroy them.
  for (const auto& rewriter : rewriters_) {
    aura::Window::Windows windows = ash::Shell::GetAllRootWindows();
    for (auto* window : windows) {
      window->GetHost()->GetEventSource()->RemoveEventRewriter(rewriter.get());
    }
  }
  rewriters_.clear();
}

void EventRewriterController::AddEventRewriter(
    std::unique_ptr<ui::EventRewriter> rewriter) {
  DCHECK(!initialized_);
  rewriters_.push_back(std::move(rewriter));
}

void EventRewriterController::Init() {
  DCHECK(!initialized_);
  initialized_ = true;
  // Add the rewriters to each existing root window EventSource.
  aura::Window::Windows windows = ash::Shell::GetAllRootWindows();
  for (auto* window : windows)
    AddToEventSource(window->GetHost()->GetEventSource());

  // In case there are any mirroring displays, their hosts' EventSources won't
  // be included above.
  const auto* mirror_window_controller =
      ash::Shell::Get()->window_tree_host_manager()->mirror_window_controller();
  for (auto* window : mirror_window_controller->GetAllRootWindows())
    AddToEventSource(window->GetHost()->GetEventSource());
}

void EventRewriterController::OnHostInitialized(aura::WindowTreeHost* host) {
  if (initialized_)
    AddToEventSource(host->GetEventSource());
}

void EventRewriterController::AddToEventSource(ui::EventSource* source) {
  DCHECK(source);
  for (const auto& rewriter : rewriters_) {
    source->AddEventRewriter(rewriter.get());
  }
}

}  // namespace chromeos
