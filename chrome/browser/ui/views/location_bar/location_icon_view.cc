// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_scaled_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

LocationIconView::LocationIconView(const gfx::FontList& font_list,
                                   SkColor parent_background_color,
                                   LocationBarView* location_bar)
    : IconLabelBubbleView(IDR_OMNIBOX_HTTPS_INVALID,
                          font_list,
                          parent_background_color,
                          true),
      suppress_mouse_released_action_(false),
      location_bar_(location_bar) {
  set_id(VIEW_ID_LOCATION_ICON);
  SetFocusable(true);
  SetBackground(false);
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
    OmniboxEditModel* model = location_bar_->GetOmniboxView()->model();
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
  location_bar_->GetOmniboxView()->CloseOmniboxPopup();
  return false;
}

bool LocationIconView::OnKeyPressed(const ui::KeyEvent& event) {
  return false;
}

void LocationIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
    return;
  OnClickOrTap(*event);
  event->SetHandled();
}

bool LocationIconView::GetTooltipText(const gfx::Point& p,
                                      base::string16* tooltip) const {
  if (show_tooltip_)
    *tooltip = l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON);
  return show_tooltip_;
}

void LocationIconView::OnClickOrTap(const ui::LocatedEvent& event) {
  // Do not show page info if the user has been editing the location bar or the
  // location bar is at the NTP.
  if (location_bar_->GetOmniboxView()->IsEditingOrEmpty())
    return;
  ProcessLocatedEvent(event);
}

void LocationIconView::ProcessLocatedEvent(const ui::LocatedEvent& event) {
  if (HitTestPoint(event.location()))
    OnActivate();
}

gfx::Size LocationIconView::GetMinimumSize() const {
  return GetMinimumSizeForPreferredSize(GetPreferredSize());
}

gfx::Size LocationIconView::GetMinimumSizeForLabelText(
    const base::string16& text) const {
  views::Label label(text, font_list());
  return GetMinimumSizeForPreferredSize(
      GetSizeForLabelWidth(label.GetPreferredSize().width()));
}

SkColor LocationIconView::GetTextColor() const {
  return location_bar_->GetColor(LocationBarView::EV_BUBBLE_TEXT_AND_BORDER);
}

SkColor LocationIconView::GetBorderColor() const {
  return GetTextColor();
}

bool LocationIconView::OnActivate() {
  WebContents* contents = location_bar_->GetWebContents();
  if (!contents)
    return false;

  // Important to use GetVisibleEntry to match what's showing in the omnibox.
  NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  // The visible entry can be nullptr in the case of window.open("").
  if (!entry)
    return false;

  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(contents);
  DCHECK(model_client);

  location_bar_->delegate()->ShowWebsiteSettings(
      contents, entry->GetURL(), model_client->GetSecurityInfo());
  return true;
}

gfx::Size LocationIconView::GetMinimumSizeForPreferredSize(
    gfx::Size size) const {
  const int kMinCharacters = 10;
  size.SetToMin(
      GetSizeForLabelWidth(font_list().GetExpectedTextWidth(kMinCharacters)));
  return size;
}

void LocationIconView::SetBackground(bool should_show_ev) {
  static const int kEvBackgroundImages[] = IMAGE_GRID(IDR_OMNIBOX_EV_BUBBLE);
  if (should_show_ev)
    SetBackgroundImageGrid(kEvBackgroundImages);
  else
    UnsetBackgroundImageGrid();
}
