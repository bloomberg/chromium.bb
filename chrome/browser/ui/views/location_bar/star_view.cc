// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

StarView::StarView(CommandUpdater* command_updater)
    : command_updater_(command_updater),
      suppress_mouse_released_action_(false) {
  set_id(VIEW_ID_STAR_BUTTON);
  SetToggled(false);
  set_accessibility_focusable(true);
  TouchableLocationBarView::Init(this);
}

StarView::~StarView() {
}

void StarView::SetToggled(bool on) {
  SetTooltipText(l10n_util::GetStringUTF16(
      on ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR));
  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      on ? IDR_STAR_LIT : IDR_STAR));
}

int StarView::GetBuiltInHorizontalPadding() const {
  return GetBuiltInHorizontalPaddingImpl();
}

void StarView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_STAR);
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
}

bool StarView::GetTooltipText(const gfx::Point& p, string16* tooltip) const {
  // Don't show tooltip to distract user if BookmarkBubbleView is showing.
  if (chrome::IsBookmarkBubbleViewShowing())
    return false;

  return views::ImageView::GetTooltipText(p, tooltip);
}

bool StarView::OnMousePressed(const ui::MouseEvent& event) {
  // If the bookmark bubble is showing then don't reshow it when the mouse is
  // released.
  suppress_mouse_released_action_ = chrome::IsBookmarkBubbleViewShowing();

  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void StarView::OnMouseReleased(const ui::MouseEvent& event) {
  // If this is the second click on this view then the bookmark bubble was
  // showing on the mouse pressed event and is hidden now. Prevent the bubble
  // from reshowing by doing nothing here.
  if (suppress_mouse_released_action_) {
    suppress_mouse_released_action_ = false;
    return;
  }

  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location())) {
    UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint",
                              bookmark_utils::ENTRY_POINT_STAR_MOUSE,
                              bookmark_utils::ENTRY_POINT_LIMIT);
    command_updater_->ExecuteCommand(IDC_BOOKMARK_PAGE_FROM_STAR);
  }
}

bool StarView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint",
                              bookmark_utils::ENTRY_POINT_STAR_KEY,
                              bookmark_utils::ENTRY_POINT_LIMIT);
    command_updater_->ExecuteCommand(IDC_BOOKMARK_PAGE_FROM_STAR);
    return true;
  }
  return false;
}

void StarView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint",
                              bookmark_utils::ENTRY_POINT_STAR_GESTURE,
                              bookmark_utils::ENTRY_POINT_LIMIT);
    command_updater_->ExecuteCommand(IDC_BOOKMARK_PAGE_FROM_STAR);
    event->SetHandled();
  }
}
