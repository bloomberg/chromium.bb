// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shadow_controller.h"

#include "ash/mus/shadow.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"

namespace ash {
namespace mus {
namespace {

// Returns the first ancestor of |from| (including |from|) that has a shadow.
aura::Window* FindAncestorWithShadow(aura::Window* from) {
  aura::Window* result = from;
  while (result && !Shadow::Get(result))
    result = result->parent();
  // Small shadows never change.
  return result && Shadow::Get(result)->style() != Shadow::STYLE_SMALL
             ? result
             : nullptr;
}

}  // namespace

ShadowController::ShadowController() {
  aura::Env::GetInstance()->AddObserver(this);
  SetFocusClient(aura::Env::GetInstance()->active_focus_client());
}

ShadowController::~ShadowController() {
  aura::Env::GetInstance()->RemoveObserver(this);
  if (active_window_)
    active_window_->RemoveObserver(this);
  if (active_focus_client_)
    active_focus_client_->RemoveObserver(this);
}

void ShadowController::SetActiveWindow(aura::Window* window) {
  window = FindAncestorWithShadow(window);
  if (window == active_window_)
    return;

  if (active_window_) {
    if (Shadow::Get(active_window_))
      Shadow::Get(active_window_)->SetStyle(Shadow::STYLE_INACTIVE);
    active_window_->RemoveObserver(this);
  }
  active_window_ = window;
  if (active_window_) {
    Shadow::Get(active_window_)->SetStyle(Shadow::STYLE_ACTIVE);
    active_window_->AddObserver(this);
  }
}

void ShadowController::SetFocusClient(aura::client::FocusClient* focus_client) {
  if (active_focus_client_)
    active_focus_client_->RemoveObserver(this);
  active_focus_client_ = focus_client;
  if (active_focus_client_) {
    active_focus_client_->AddObserver(this);
    SetActiveWindow(active_focus_client_->GetFocusedWindow());
  } else {
    SetActiveWindow(nullptr);
  }
}

void ShadowController::OnWindowInitialized(aura::Window* window) {}

void ShadowController::OnActiveFocusClientChanged(
    aura::client::FocusClient* focus_client,
    aura::Window* window) {
  SetFocusClient(focus_client);
}

void ShadowController::OnWindowFocused(aura::Window* gained_focus,
                                       aura::Window* lost_focus) {
  SetActiveWindow(gained_focus);
}

void ShadowController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, active_window_);
  SetActiveWindow(nullptr);
}

}  // namespace mus
}  // namespace ash
