// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/mic_search_view.h"

#include "chrome/browser/ui/view_ids.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

MicSearchView::MicSearchView(views::ButtonListener* button_listener)
    : views::ImageButton(button_listener) {
  set_id(VIEW_ID_MIC_SEARCH_BUTTON);
  set_accessibility_focusable(true);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_MIC_SEARCH));
  SetImage(STATE_NORMAL,
           ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
               IDR_OMNIBOX_MIC_SEARCH));
  SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
  TouchableLocationBarView::Init(this);
}

MicSearchView::~MicSearchView() {
}

int MicSearchView::GetBuiltInHorizontalPadding() const {
  return GetBuiltInHorizontalPaddingImpl();
}
