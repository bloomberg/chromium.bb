// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/transience_manager.h"

namespace vr {

TransienceManager::TransienceManager(UiElement* element,
                                     float opacity_when_enabled,
                                     const base::TimeDelta& timeout)
    : element_(element),
      opacity_when_enabled_(opacity_when_enabled),
      timeout_(timeout) {
  element_->SetVisible(false);
}

void TransienceManager::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  if (enabled) {
    Show();
    StartTimer();
  } else {
    Hide();
    visibility_timer_.Stop();
  }
}

void TransienceManager::KickVisibilityIfEnabled() {
  if (enabled_) {
    Show();
    StartTimer();
  }
}

void TransienceManager::EndVisibilityIfEnabled() {
  if (enabled_) {
    Hide();
    visibility_timer_.Stop();
  }
}

void TransienceManager::StartTimer() {
  visibility_timer_.Start(
      FROM_HERE, timeout_,
      base::Bind(&TransienceManager::OnTimeout, base::Unretained(this)));
}

void TransienceManager::OnTimeout() {
  Hide();
}

void TransienceManager::Show() {
  element_->SetVisible(true);
  element_->SetOpacity(opacity_when_enabled_);
}

void TransienceManager::Hide() {
  element_->SetVisible(false);
  element_->SetOpacity(0);
}

}  // namespace vr
