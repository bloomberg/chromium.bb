// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/overlays/test/fake_overlay_ui_dismissal_delegate.h"

FakeDismissalDelegate::FakeDismissalDelegate() = default;
FakeDismissalDelegate::~FakeDismissalDelegate() = default;

bool FakeDismissalDelegate::HasUIBeenDismissed(OverlayRequest* request) const {
  return dismissed_requests_.find(request) != dismissed_requests_.end();
}

void FakeDismissalDelegate::OverlayUIDidFinishDismissal(
    OverlayRequest* request) {
  dismissed_requests_.insert(request);
}
