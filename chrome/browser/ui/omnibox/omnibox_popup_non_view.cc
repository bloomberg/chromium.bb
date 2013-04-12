// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_popup_non_view.h"

#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

OmniboxPopupNonView::OmniboxPopupNonView(OmniboxEditModel* edit_model)
  : model_(this, edit_model),
    is_open_(false) {
}

OmniboxPopupNonView::~OmniboxPopupNonView() {
}

bool OmniboxPopupNonView::IsOpen() const {
  return is_open_;
}

void OmniboxPopupNonView::InvalidateLine(size_t line) {
}

void OmniboxPopupNonView::UpdatePopupAppearance() {
  is_open_ = !model_.result().empty();
}

gfx::Rect OmniboxPopupNonView::GetTargetBounds() {
  // Non-view bounds never obscure the page, so return an empty rect.
  return gfx::Rect();
}

void OmniboxPopupNonView::PaintUpdatesNow() {
  // TODO(beng): remove this from the interface.
}

void OmniboxPopupNonView::OnDragCanceled() {
}
