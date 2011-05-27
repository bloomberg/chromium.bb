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

bool ViewsEulaScreenActor::IsUsageStatsChecked() const {
  return view() && view()->IsUsageStatsChecked();
}

void ViewsEulaScreenActor::SetDelegate(Delegate* delegate) {
  screen_ = delegate;
}

}  // namespace chromeos
