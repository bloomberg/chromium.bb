// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/label_tray_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Maps a non-MD PNG resource id to its corresponding MD vector icon.
// TODO(tdanderson): Remove this once material design is enabled by
// default. See crbug.com/614453.
const gfx::VectorIcon& ResourceIdToVectorIcon(int resource_id) {
  switch (resource_id) {
    case IDR_AURA_UBER_TRAY_ENTERPRISE:
      return kSystemMenuBusinessIcon;
    case IDR_AURA_UBER_TRAY_BUBBLE_SESSION_LENGTH_LIMIT:
      return kSystemMenuTimerIcon;
    case IDR_AURA_UBER_TRAY_CHILD_USER:
      return kSystemMenuChildUserIcon;
    case IDR_AURA_UBER_TRAY_SUPERVISED_USER:
      return kSystemMenuSupervisedUserIcon;
    default:
      NOTREACHED();
      break;
  }
  return gfx::kNoneIcon;
}

}  // namespace

LabelTrayView::LabelTrayView(ViewClickListener* click_listener,
                             int icon_resource_id)
    : click_listener_(click_listener), icon_resource_id_(icon_resource_id) {
  SetLayoutManager(new views::FillLayout());
  SetVisible(false);
}

LabelTrayView::~LabelTrayView() {}

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
    const bool use_md = MaterialDesignController::IsSystemTrayMenuMaterial();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::ImageSkia icon =
        use_md ? gfx::CreateVectorIcon(
                     ResourceIdToVectorIcon(icon_resource_id_), kMenuIconColor)
               : *rb.GetImageSkiaNamed(icon_resource_id_);
    child->AddIconAndLabelForDefaultView(icon, message, false /* highlight */);
    child->text_label()->SetMultiLine(true);
    if (!use_md) {
      child->SetBorder(views::CreateEmptyBorder(
          0, kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingHorizontal));
      child->text_label()->SizeToFit(kTrayNotificationContentsWidth);
    }
  } else {
    child->AddLabel(message, gfx::ALIGN_LEFT, false /* highlight */);
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
