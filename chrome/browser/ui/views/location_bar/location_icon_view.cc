// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

using content::WebContents;

LocationIconView::LocationIconView(const gfx::FontList& font_list,
                                   LocationBarView* location_bar)
    : IconLabelBubbleView(font_list, true),
      suppress_mouse_released_action_(false),
      location_bar_(location_bar),
      animation_(this) {
  set_id(VIEW_ID_LOCATION_ICON);

#if defined(OS_MACOSX)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif

  animation_.SetSlideDuration(kOpenTimeMS);
}

LocationIconView::~LocationIconView() {
}

gfx::Size LocationIconView::GetMinimumSize() const {
  return GetMinimumSizeForPreferredSize(GetPreferredSize());
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

  suppress_mouse_released_action_ =
      WebsiteSettingsPopupView::GetShownPopupType() !=
      WebsiteSettingsPopupView::POPUP_NONE;
  return true;
}

bool LocationIconView::OnMouseDragged(const ui::MouseEvent& event) {
  location_bar_->GetOmniboxView()->CloseOmniboxPopup();
  return false;
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

SkColor LocationIconView::GetTextColor() const {
  return location_bar_->GetColor(LocationBarView::SECURITY_CHIP_TEXT);
}

bool LocationIconView::OnActivate(const ui::Event& event) {
  WebContents* contents = location_bar_->GetWebContents();
  if (!contents)
    return false;
  location_bar_->delegate()->ShowWebsiteSettings(contents);
  return true;
}

void LocationIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  IconLabelBubbleView::GetAccessibleNodeData(node_data);
  node_data->role = ui::AX_ROLE_POP_UP_BUTTON;
}

gfx::Size LocationIconView::GetMinimumSizeForLabelText(
    const base::string16& text) const {
  views::Label label(text, font_list());
  return GetMinimumSizeForPreferredSize(
      GetSizeForLabelWidth(label.GetPreferredSize().width()));
}

void LocationIconView::SetTextVisibility(bool should_show,
                                         bool should_animate) {
  if (!should_animate) {
    animation_.Reset(should_show);
  } else if (should_show) {
    animation_.Show();
  } else {
    animation_.Hide();
  }
  // The label text color may have changed.
  OnNativeThemeChanged(GetNativeTheme());
}

double LocationIconView::WidthMultiplier() const {
  return animation_.GetCurrentValue();
}

void LocationIconView::AnimationProgressed(const gfx::Animation*) {
  location_bar_->Layout();
  location_bar_->SchedulePaint();
}

void LocationIconView::ProcessLocatedEvent(const ui::LocatedEvent& event) {
  if (HitTestPoint(event.location()))
    OnActivate(event);
}

gfx::Size LocationIconView::GetMinimumSizeForPreferredSize(
    gfx::Size size) const {
  const int kMinCharacters = 10;
  size.SetToMin(
      GetSizeForLabelWidth(font_list().GetExpectedTextWidth(kMinCharacters)));
  return size;
}

void LocationIconView::OnClickOrTap(const ui::LocatedEvent& event) {
  // Do not show page info if the user has been editing the location bar or the
  // location bar is at the NTP.
  if (location_bar_->GetOmniboxView()->IsEditingOrEmpty())
    return;
  ProcessLocatedEvent(event);
}
