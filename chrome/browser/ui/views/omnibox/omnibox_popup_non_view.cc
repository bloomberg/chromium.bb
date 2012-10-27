// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_non_view.h"

#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

OmniboxPopupNonView::OmniboxPopupNonView(OmniboxEditModel* edit_model)
  : model_(this, edit_model) {
}

OmniboxPopupNonView::~OmniboxPopupNonView() {
}

bool OmniboxPopupNonView::IsOpen() const {
  return !model_.result().empty();
}

void OmniboxPopupNonView::InvalidateLine(size_t line) {
}

void OmniboxPopupNonView::UpdatePopupAppearance() {
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

bool OmniboxPopupNonView::IsSelectedIndex(size_t index) const {
  return index == model_.selected_line();
}

bool OmniboxPopupNonView::IsHoveredIndex(size_t index) const {
  return index == model_.hovered_line();
}

gfx::Image OmniboxPopupNonView::GetIconIfExtensionMatch(size_t index) const {
  if (index < model_.result().size())
    return model_.GetIconIfExtensionMatch(model_.result().match_at(index));
  return gfx::Image();
}
