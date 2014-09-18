// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LocationIconView::LocationIconView(LocationBarView* location_bar)
    : suppress_mouse_released_action_(false),
      page_info_helper_(this, location_bar) {
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
        page_info_helper_.location_bar()->GetOmniboxView()->model();
    if (model->CanPasteAndGo(text))
      model->PasteAndGo(text);
  }

  suppress_mouse_released_action_ = WebsiteSettingsPopupView::IsPopupShowing();
  return true;
}

void LocationIconView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyMiddleMouseButton())
    return;

  // If this is the second click on this view then the bubble was showing on
  // the mouse pressed event and is hidden now. Prevent the bubble from
  // reshowing by doing nothing here.
  if (suppress_mouse_released_action_) {
    suppress_mouse_released_action_ = false;
    return;
  }

  OnClickOrTap(event);
}

bool LocationIconView::OnMouseDragged(const ui::MouseEvent& event) {
  page_info_helper_.location_bar()->GetOmniboxView()->CloseOmniboxPopup();
  return false;
}

void LocationIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return;
  OnClickOrTap(*event);
  event->SetHandled();
}

void LocationIconView::OnClickOrTap(const ui::LocatedEvent& event) {
  // Do not show page info if the user has been editing the location bar or the
  // location bar is at the NTP.
  if (page_info_helper_.location_bar()->GetOmniboxView()->IsEditingOrEmpty())
    return;

  page_info_helper_.ProcessEvent(event);
}

void LocationIconView::ShowTooltip(bool show) {
  SetTooltipText(show ?
      l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON) : base::string16());
}
