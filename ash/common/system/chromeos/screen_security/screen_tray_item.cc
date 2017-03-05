// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/screen_security/screen_tray_item.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace tray {

// ScreenTrayView implementations.
ScreenTrayView::ScreenTrayView(ScreenTrayItem* screen_tray_item)
    : TrayItemView(screen_tray_item), screen_tray_item_(screen_tray_item) {
  CreateImageView();
  if (MaterialDesignController::UseMaterialDesignSystemIcons()) {
    image_view()->SetImage(
        gfx::CreateVectorIcon(kSystemTrayScreenShareIcon, kTrayIconColor));
  } else {
    image_view()->SetImage(ui::ResourceBundle::GetSharedInstance()
                               .GetImageNamed(IDR_AURA_UBER_TRAY_SCREENSHARE)
                               .ToImageSkia());
  }
  Update();
}

ScreenTrayView::~ScreenTrayView() {}

void ScreenTrayView::Update() {
  SetVisible(screen_tray_item_->is_started());
}

// ScreenStatusView implementations.
ScreenStatusView::ScreenStatusView(ScreenTrayItem* screen_tray_item,
                                   const base::string16& label_text,
                                   const base::string16& stop_button_text)
    : screen_tray_item_(screen_tray_item),
      icon_(nullptr),
      label_(nullptr),
      stop_button_(nullptr),
      label_text_(label_text),
      stop_button_text_(stop_button_text) {
  CreateItems();
  TriView* tri_view(TrayPopupUtils::CreateDefaultRowView());
  SetLayoutManager(new views::FillLayout);
  AddChildView(tri_view);
  tri_view->AddView(TriView::Container::START, icon_);
  // TODO(bruthig): Multiline Labels don't lay out well with borders so we add
  // the border to the Label's container instead. See https://crbug.com/678337 &
  // https://crbug.com/682221.
  tri_view->SetContainerBorder(
      TriView::Container::CENTER,
      views::CreateEmptyBorder(0, 0, 0, kTrayPopupLabelRightPadding));
  tri_view->AddView(TriView::Container::CENTER, label_);
  tri_view->AddView(TriView::Container::END, stop_button_);
  tri_view->SetContainerBorder(
      TriView::Container::END,
      views::CreateEmptyBorder(0, 0, 0, kTrayPopupButtonEndMargin));
  if (screen_tray_item_)
    UpdateFromScreenTrayItem();
}

ScreenStatusView::~ScreenStatusView() {}

void ScreenStatusView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  DCHECK(sender == stop_button_);
  screen_tray_item_->Stop();
  screen_tray_item_->RecordStoppedFromDefaultViewMetric();
}

void ScreenStatusView::CreateItems() {
  const bool use_md = MaterialDesignController::IsSystemTrayMenuMaterial();
  icon_ = TrayPopupUtils::CreateMainImageView();
  icon_->SetImage(gfx::CreateVectorIcon(
      kSystemMenuScreenShareIcon, TrayPopupItemStyle::GetIconColor(
                                      TrayPopupItemStyle::ColorStyle::ACTIVE)));
  if (!use_md) {
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    icon_->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_SCREENSHARE_DARK)
                        .ToImageSkia());
  }

  label_ = TrayPopupUtils::CreateDefaultLabel();
  label_->SetMultiLine(true);
  label_->SetText(label_text_);
  // TODO(bruthig): Multiline Labels don't lay out well with borders.
  // See https://crbug.com/678337 & https://crbug.com/682221.
  label_->SetBorder(nullptr);
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
  style.SetupLabel(label_);

  stop_button_ = TrayPopupUtils::CreateTrayPopupButton(this, stop_button_text_);
}

void ScreenStatusView::UpdateFromScreenTrayItem() {
  SetVisible(screen_tray_item_->is_started());
}

ScreenNotificationDelegate::ScreenNotificationDelegate(
    ScreenTrayItem* screen_tray)
    : screen_tray_(screen_tray) {}

ScreenNotificationDelegate::~ScreenNotificationDelegate() {}

void ScreenNotificationDelegate::ButtonClick(int button_index) {
  DCHECK_EQ(0, button_index);
  screen_tray_->Stop();
  screen_tray_->RecordStoppedFromNotificationViewMetric();
}

}  // namespace tray

ScreenTrayItem::ScreenTrayItem(SystemTray* system_tray, UmaType uma_type)
    : SystemTrayItem(system_tray, uma_type),
      tray_view_(nullptr),
      default_view_(nullptr),
      is_started_(false),
      stop_callback_(base::Bind(&base::DoNothing)) {}

ScreenTrayItem::~ScreenTrayItem() {}

void ScreenTrayItem::Update() {
  if (tray_view_)
    tray_view_->Update();
  if (default_view_)
    default_view_->UpdateFromScreenTrayItem();
  if (is_started_) {
    CreateOrUpdateNotification();
  } else {
    message_center::MessageCenter::Get()->RemoveNotification(
        GetNotificationId(), false /* by_user */);
  }
}

void ScreenTrayItem::Start(const base::Closure& stop_callback) {
  stop_callback_ = stop_callback;
  is_started_ = true;

  if (tray_view_)
    tray_view_->Update();

  if (default_view_)
    default_view_->UpdateFromScreenTrayItem();

  if (!system_tray()->HasSystemBubbleType(
          SystemTrayBubble::BUBBLE_TYPE_DEFAULT)) {
    CreateOrUpdateNotification();
  }
}

void ScreenTrayItem::Stop() {
  is_started_ = false;
  Update();

  if (stop_callback_.is_null())
    return;

  base::Closure callback = stop_callback_;
  stop_callback_.Reset();
  callback.Run();
}

views::View* ScreenTrayItem::CreateTrayView(LoginStatus status) {
  tray_view_ = new tray::ScreenTrayView(this);
  return tray_view_;
}

void ScreenTrayItem::RecordStoppedFromDefaultViewMetric() {}

void ScreenTrayItem::RecordStoppedFromNotificationViewMetric() {}

void ScreenTrayItem::DestroyTrayView() {
  tray_view_ = nullptr;
}

void ScreenTrayItem::DestroyDefaultView() {
  default_view_ = nullptr;
}

}  // namespace ash
