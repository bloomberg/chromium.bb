// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/label_tray_view.h"

#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/view_click_listener.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

LabelTrayView::LabelTrayView(ViewClickListener* click_listener,
                             int icon_resource_id)
    : click_listener_(click_listener),
      icon_resource_id_(icon_resource_id) {
  SetLayoutManager(new views::FillLayout());
  SetVisible(false);
}

LabelTrayView::~LabelTrayView() {
}

void LabelTrayView::SetMessage(const base::string16& message) {
  if (message_ == message)
    return;

  message_ = message;
  RemoveAllChildViews(true);
  if (!message_.empty()) {
    AddChildView(CreateChildView(message_));
    SetVisible(true);
  } else {
    SetVisible(false);
  }
}

views::View* LabelTrayView::CreateChildView(
    const base::string16& message) const {
  HoverHighlightView* child = new HoverHighlightView(click_listener_);
  if (icon_resource_id_) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* icon = rb.GetImageSkiaNamed(icon_resource_id_);
    child->AddIconAndLabel(*icon, message, gfx::Font::NORMAL);
    child->SetBorder(views::Border::CreateEmptyBorder(
        0, kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingHorizontal));
    child->text_label()->SetMultiLine(true);
    child->text_label()->SizeToFit(kTrayNotificationContentsWidth);
  } else {
    child->AddLabel(message, gfx::ALIGN_LEFT, gfx::Font::NORMAL);
    child->text_label()->SetMultiLine(true);
    child->text_label()->SizeToFit(kTrayNotificationContentsWidth +
                                   kNotificationIconWidth);
  }
  child->text_label()->SetAllowCharacterBreak(true);
  child->SetExpandable(true);
  child->SetVisible(true);
  return child;
}

}  // namespace ash
