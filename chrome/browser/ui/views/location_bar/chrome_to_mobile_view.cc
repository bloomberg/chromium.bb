// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/chrome_to_mobile_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ChromeToMobileView::ChromeToMobileView(
    LocationBarView* location_bar_view,
    CommandUpdater* command_updater)
    : location_bar_view_(location_bar_view),
      command_updater_(command_updater) {
  set_id(VIEW_ID_CHROME_TO_MOBILE_BUTTON);
  set_accessibility_focusable(true);
  SetImage(ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_MOBILE));
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_CHROME_TO_MOBILE_BUBBLE_TOOLTIP));
  SetVisible(command_updater_->IsCommandEnabled(IDC_CHROME_TO_MOBILE_PAGE));
  command_updater_->AddCommandObserver(IDC_CHROME_TO_MOBILE_PAGE, this);
}

ChromeToMobileView::~ChromeToMobileView() {
  command_updater_->RemoveCommandObserver(IDC_CHROME_TO_MOBILE_PAGE, this);
}

void ChromeToMobileView::EnabledStateChangedForCommand(int id, bool enabled) {
  DCHECK_EQ(id, IDC_CHROME_TO_MOBILE_PAGE);
  if (enabled != visible()) {
    SetVisible(enabled);
    location_bar_view_->Update(NULL);
  }
}

void ChromeToMobileView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_CHROME_TO_MOBILE);
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
}

bool ChromeToMobileView::GetTooltipText(const gfx::Point& p,
                                        string16* tooltip) const {
  // Don't show tooltip to distract user if ChromeToMobileBubbleView is showing.
  if (browser::IsChromeToMobileBubbleViewShowing())
    return false;

  return ImageView::GetTooltipText(p, tooltip);
}

bool ChromeToMobileView::OnMousePressed(const views::MouseEvent& event) {
  // Show the bubble on mouse release; that is standard button behavior.
  return true;
}

void ChromeToMobileView::OnMouseReleased(const views::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTest(event.location()))
    command_updater_->ExecuteCommand(IDC_CHROME_TO_MOBILE_PAGE);
}

bool ChromeToMobileView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    command_updater_->ExecuteCommand(IDC_CHROME_TO_MOBILE_PAGE);
    return true;
  }
  return false;
}
