// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LocationIconView::LocationIconView(LocationBarView* location_bar)
    : page_info_helper_(this, location_bar) {
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
}

LocationIconView::~LocationIconView() {
}

bool LocationIconView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton() &&
      ui::Clipboard::IsSupportedClipboardType(ui::CLIPBOARD_TYPE_SELECTION)) {
    base::string16 text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::CLIPBOARD_TYPE_SELECTION, &text);
    text = OmniboxView::SanitizeTextForPaste(text);
    OmniboxEditModel* model =
        page_info_helper_.location_bar()->omnibox_view()->model();
    if (model->CanPasteAndGo(text))
      model->PasteAndGo(text);
  }

  // Showing the bubble on mouse release is standard button behavior.
  return true;
}

void LocationIconView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton())
    return;
  OnClickOrTap(event);
}

void LocationIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return;
  OnClickOrTap(*event);
  event->SetHandled();
}

void LocationIconView::OnClickOrTap(const ui::LocatedEvent& event) {
  // Do not show page info if the user has been editing the location bar or the
  // location bar is at the NTP.  Also skip showing the page info if the
  // toolbar-based origin chip is being shown because it is responsible for
  // showing the page info instead.
  if (page_info_helper_.location_bar()->omnibox_view()->IsEditingOrEmpty() ||
      chrome::ShouldDisplayOriginChip())
    return;

  page_info_helper_.ProcessEvent(event);
}

void LocationIconView::ShowTooltip(bool show) {
  SetTooltipText(show ?
      l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON) : base::string16());
}
