// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_info_label.h"

#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

TrayInfoLabel::TrayInfoLabel(TrayInfoLabel::Delegate* delegate, int message_id)
    : ActionableView(nullptr /* owner */, TrayPopupInkDropStyle::FILL_BOUNDS),
      label_(TrayPopupUtils::CreateDefaultLabel()),
      message_id_(message_id),
      delegate_(delegate) {
  SetLayoutManager(new views::FillLayout);

  TriView* tri_view = TrayPopupUtils::CreateMultiTargetRowView();
  tri_view->SetInsets(gfx::Insets(0,
                                  kMenuExtraMarginFromLeftEdge +
                                      kTrayPopupPaddingHorizontal -
                                      kTrayPopupLabelHorizontalPadding,
                                  0, kTrayPopupPaddingHorizontal));
  tri_view->SetContainerVisible(TriView::Container::START, false);
  tri_view->SetContainerVisible(TriView::Container::END, false);
  tri_view->AddView(TriView::Container::CENTER, label_);

  AddChildView(tri_view);

  Update(message_id);
}

void TrayInfoLabel::Update(int message_id) {
  message_id_ = message_id;

  TrayPopupItemStyle::FontStyle font_style;

  if (IsClickable()) {
    SetInkDropMode(InkDropHostView::InkDropMode::ON);
    font_style = TrayPopupItemStyle::FontStyle::CLICKABLE_SYSTEM_INFO;
  } else {
    SetInkDropMode(InkDropHostView::InkDropMode::OFF);
    font_style = TrayPopupItemStyle::FontStyle::SYSTEM_INFO;
  }

  const TrayPopupItemStyle style(font_style);
  style.SetupLabel(label_);

  base::string16 text = l10n_util::GetStringUTF16(message_id);
  label_->SetText(text);
  SetAccessibleName(text);
}

bool TrayInfoLabel::PerformAction(const ui::Event& event) {
  if (IsClickable()) {
    delegate_->OnLabelClicked(message_id_);
    return true;
  }
  return false;
}

void TrayInfoLabel::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);

  if (!IsClickable())
    node_data->role = ui::AX_ROLE_LABEL_TEXT;
}

bool TrayInfoLabel::IsClickable() {
  if (delegate_)
    return delegate_->IsLabelClickable(message_id_);
  return false;
}

}  // namespace ash
