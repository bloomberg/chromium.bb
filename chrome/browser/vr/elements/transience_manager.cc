// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/transience_manager.h"

namespace vr {

TransienceManager::TransienceManager(UiElement* element,
                                     const base::TimeDelta& timeout)
    : element_(element), timeout_(timeout) {
  element_->SetVisible(false);
}

void TransienceManager::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  if (enabled) {
    element_->SetVisible(true);
    StartTimer();
  } else {
    element_->SetVisible(false);
    visibility_timer_.Stop();
  }
}

void TransienceManager::KickVisibilityIfEnabled() {
  if (enabled_) {
    element_->SetVisible(true);
    StartTimer();
  }
}

void TransienceManager::EndVisibilityIfEnabled() {
  if (enabled_) {
    element_->SetVisible(false);
    visibility_timer_.Stop();
  }
}

void TransienceManager::StartTimer() {
  visibility_timer_.Start(
      FROM_HERE, timeout_,
      base::Bind(&TransienceManager::OnTimeout, base::Unretained(this)));
}

void TransienceManager::OnTimeout() {
  element_->SetVisible(false);
  element_->SetOpacity(0);
}

}  // namespace vr
