// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shadow_controller.h"

#include "ash/mus/property_util.h"
#include "ash/mus/shadow.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"

namespace ash {
namespace mus {
namespace {

// Returns the first ancestor of |from| (including |from|) that has a shadow.
::ui::Window* FindAncestorWithShadow(::ui::Window* from) {
  ::ui::Window* result = from;
  while (result && !GetShadow(result))
    result = result->parent();
  // Small shadows never change.
  return result && GetShadow(result)->style() != Shadow::STYLE_SMALL ? result
                                                                     : nullptr;
}

}  // namespace

ShadowController::ShadowController(::ui::WindowTreeClient* window_tree)
    : window_tree_(window_tree), active_window_(nullptr) {
  window_tree_->AddObserver(this);
  SetActiveWindow(FindAncestorWithShadow(window_tree_->GetFocusedWindow()));
}

ShadowController::~ShadowController() {
  window_tree_->RemoveObserver(this);
  if (active_window_)
    active_window_->RemoveObserver(this);
}

void ShadowController::SetActiveWindow(::ui::Window* window) {
  if (window == active_window_)
    return;

  if (active_window_) {
    if (GetShadow(active_window_))
      GetShadow(active_window_)->SetStyle(Shadow::STYLE_INACTIVE);
    active_window_->RemoveObserver(this);
  }
  active_window_ = window;
  if (active_window_) {
    GetShadow(active_window_)->SetStyle(Shadow::STYLE_ACTIVE);
    active_window_->AddObserver(this);
  }
}

void ShadowController::OnWindowTreeFocusChanged(::ui::Window* gained_focus,
                                                ::ui::Window* lost_focus) {
  SetActiveWindow(FindAncestorWithShadow(gained_focus));
}

void ShadowController::OnWindowDestroying(::ui::Window* window) {
  DCHECK_EQ(window, active_window_);
  SetActiveWindow(nullptr);
}

}  // namespace mus
}  // namespace ash
