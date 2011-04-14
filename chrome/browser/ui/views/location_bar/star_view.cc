// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

StarView::StarView(CommandUpdater* command_updater)
    : command_updater_(command_updater) {
  SetID(VIEW_ID_STAR_BUTTON);
  SetToggled(false);
  set_accessibility_focusable(true);
}

StarView::~StarView() {
}

void StarView::SetToggled(bool on) {
  SetTooltipText(UTF16ToWide(l10n_util::GetStringUTF16(
      on ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR)));
  SetImage(ResourceBundle::GetSharedInstance().GetBitmapNamed(
      on ? IDR_STAR_LIT : IDR_STAR));
}

void StarView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_STAR);
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
}

bool StarView::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  // Don't show tooltip to distract user if BookmarkBubbleView is showing.
  if (browser::IsBookmarkBubbleViewShowing())
    return false;

  return ImageView::GetTooltipText(p, tooltip);
}

bool StarView::OnMousePressed(const views::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void StarView::OnMouseReleased(const views::MouseEvent& event) {
  if (HitTest(event.location()))
    command_updater_->ExecuteCommand(IDC_BOOKMARK_PAGE);
}

bool StarView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    command_updater_->ExecuteCommand(IDC_BOOKMARK_PAGE);
    return true;
  }
  return false;
}

void StarView::BubbleClosing(Bubble* bubble, bool closed_by_escape) {
}

bool StarView::CloseOnEscape() {
  return true;
}

bool StarView::FadeInOnShow() {
  return false;
}
