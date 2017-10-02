// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_row_title_view.h"

#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

const int kLineHeightTitle = 4;
const int kLineHeightSubtitle = 3;

}  // namespace

NetworkRowTitleView::NetworkRowTitleView(int title_message_id)
    : title_(TrayPopupUtils::CreateDefaultLabel()),
      subtitle_(TrayPopupUtils::CreateDefaultLabel()) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::Orientation::kVertical));

  TrayPopupItemStyle title_style(TrayPopupItemStyle::FontStyle::SUB_HEADER);
  title_style.SetupLabel(title_);
  subtitle_->SetLineHeight(kLineHeightTitle);
  title_->SetText(l10n_util::GetStringUTF16(title_message_id));
  AddChildView(title_);

  TrayPopupItemStyle subtitle_style(TrayPopupItemStyle::FontStyle::SYSTEM_INFO);
  subtitle_style.SetupLabel(subtitle_);
  subtitle_->SetMultiLine(true);
  subtitle_->SetLineHeight(kLineHeightSubtitle);
  subtitle_->SetVisible(false);
  AddChildView(subtitle_);
}

NetworkRowTitleView::~NetworkRowTitleView() {}

void NetworkRowTitleView::SetSubtitle(int subtitle_message_id) {
  if (subtitle_message_id) {
    subtitle_->SetText(l10n_util::GetStringUTF16(subtitle_message_id));
    subtitle_->SetVisible(true);
    return;
  }

  subtitle_->SetText(base::string16());
  subtitle_->SetVisible(false);
}

}  // namespace ash
