// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/action_box_button_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ActionBoxButtonView::ActionBoxButtonView()
    : views::MenuButton(NULL, string16(), this, false) {
  set_id(VIEW_ID_ACTION_BOX_BUTTON);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_ACTION_BOX_BUTTON));
  SetImages();
  set_accessibility_focusable(true);
  set_border(NULL);
}

ActionBoxButtonView::~ActionBoxButtonView() {
}

void ActionBoxButtonView::SetImages() {
  SetIcon(*ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_ACTION_BOX_BUTTON));
  SetHoverIcon(*ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_ACTION_BOX_BUTTON_H));
  SetPushedIcon(*ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_ACTION_BOX_BUTTON_P));
}

void ActionBoxButtonView::GetAccessibleState(ui::AccessibleViewState* state) {
  MenuButton::GetAccessibleState(state);
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_ACTION_BOX_BUTTON);
}

void ActionBoxButtonView::OnMenuButtonClicked(View* source,
                                              const gfx::Point& point) {
  // TODO(yefim): Implement menu here.
}

