// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/views_update_screen_actor.h"

namespace {

// Considering 10px shadow from each side.
const int kUpdateScreenWidth = 580;
const int kUpdateScreenHeight = 305;

}  // namespace

namespace chromeos {

ViewsUpdateScreenActor::ViewsUpdateScreenActor(ViewScreenDelegate* delegate)
    : DefaultViewScreen<chromeos::UpdateView>(delegate,
                                              kUpdateScreenWidth,
                                              kUpdateScreenWidth) {
}

void ViewsUpdateScreenActor::PrepareToShow() {
  DefaultViewScreen<chromeos::UpdateView>::PrepareToShow();
}

void ViewsUpdateScreenActor::Show() {
  DefaultViewScreen<chromeos::UpdateView>::Show();
}

void ViewsUpdateScreenActor::Hide() {
  DefaultViewScreen<chromeos::UpdateView>::Hide();
}

void ViewsUpdateScreenActor::ShowManualRebootInfo() {
  view()->ShowManualRebootInfo();
}

void ViewsUpdateScreenActor::SetProgress(int progress) {
  view()->SetProgress(progress);
}

void ViewsUpdateScreenActor::ShowCurtain(bool enable) {
  view()->ShowCurtain(enable);
}

}  // namespace chromeos
