// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/transience_manager.h"

namespace vr {

TransienceManager::TransienceManager(UiElement* element,
                                     const base::TimeDelta& timeout)
    : element_(element),
      timeout_(timeout) {
  element_->SetOpacity(0);
}

void TransienceManager::SetVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }
  visible_ = visible;
  if (visible) {
    Show();
    StartTimer();
  } else {
    Hide();
    visibility_timer_.Stop();
  }
}

void TransienceManager::KickVisibility() {
  if (visible_) {
    Show();
    StartTimer();
  }
}

void TransienceManager::EndVisibility() {
  if (visible_) {
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
  element_->SetOpacity(element_->opacity_when_visible());
}

void TransienceManager::Hide() {
  element_->SetOpacity(0);
}

}  // namespace vr
