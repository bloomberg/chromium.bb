// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/views_eula_screen_actor.h"

namespace chromeos {

ViewsEulaScreenActor::ViewsEulaScreenActor(ViewScreenDelegate* delegate)
    : ViewScreen<EulaView>(delegate),
      screen_(NULL) {
}

ViewsEulaScreenActor::~ViewsEulaScreenActor() {
  if (screen_)
    screen_->OnActorDestroyed(this);
}

EulaView* ViewsEulaScreenActor::AllocateView() {
  return new EulaView(this);
}

void ViewsEulaScreenActor::PrepareToShow() {
  ViewScreen<EulaView>::PrepareToShow();
}

void ViewsEulaScreenActor::Show() {
  ViewScreen<EulaView>::Show();
}

void ViewsEulaScreenActor::Hide() {
  ViewScreen<EulaView>::Hide();
}

void ViewsEulaScreenActor::SetDelegate(Delegate* delegate) {
  screen_ = delegate;
}

void ViewsEulaScreenActor::OnPasswordFetched(const std::string& tpm_password) {
  view()->ShowTpmPassword(tpm_password);
}

}  // namespace chromeos
