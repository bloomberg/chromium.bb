// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/metro_pin_view.h"

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

MetroPinView::MetroPinView(CommandUpdater* command_updater)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ImageButton(this)),
      command_updater_(command_updater) {
  set_id(VIEW_ID_METRO_PIN);
  SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_METRO_PIN));
  TouchableLocationBarView::Init(this);
  SetIsPinned(false);
}

MetroPinView::~MetroPinView() {}

void MetroPinView::SetIsPinned(bool is_pinned) {
  SetTooltipText(l10n_util::GetStringUTF16(
      is_pinned ? IDS_TOOLTIP_METRO_PINNED : IDS_TOOLTIP_METRO_PIN));
  SetImage(BS_NORMAL, ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      is_pinned ? IDR_METRO_PINNED : IDR_METRO_PIN));
}

void MetroPinView::ButtonPressed(Button* sender, const views::Event& event) {
  command_updater_->ExecuteCommand(IDC_METRO_PIN_TO_START_SCREEN);
}

int MetroPinView::GetBuiltInHorizontalPadding() const {
  return GetBuiltInHorizontalPaddingImpl();
}
