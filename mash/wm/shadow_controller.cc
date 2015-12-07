// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/shadow_controller.h"

#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mash/wm/property_util.h"
#include "mash/wm/shadow.h"

namespace mash {
namespace wm {
namespace {

// Returns the first ancestor of |from| (including |from|) that has a shadow.
mus::Window* FindAncestorWithShadow(mus::Window* from) {
  mus::Window* result = from;
  while (result && !GetShadow(result))
    result = result->parent();
  // Small shadows never change.
  return result && GetShadow(result)->style() != Shadow::STYLE_SMALL ? result
                                                                     : nullptr;
}

}  // namespace

ShadowController::ShadowController(mus::WindowTreeConnection* window_tree)
    : window_tree_(window_tree), active_window_(nullptr) {
  window_tree_->AddObserver(this);
  SetActiveWindow(FindAncestorWithShadow(window_tree_->GetFocusedWindow()));
}

ShadowController::~ShadowController() {
  window_tree_->RemoveObserver(this);
  if (active_window_)
    active_window_->RemoveObserver(this);
}

void ShadowController::SetActiveWindow(mus::Window* window) {
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

void ShadowController::OnWindowTreeFocusChanged(mus::Window* gained_focus,
                                                mus::Window* lost_focus) {
  SetActiveWindow(FindAncestorWithShadow(gained_focus));
}

void ShadowController::OnWindowDestroying(mus::Window* window) {
  DCHECK_EQ(window, active_window_);
  SetActiveWindow(nullptr);
}

}  // namespace wm
}  // namespace mash
