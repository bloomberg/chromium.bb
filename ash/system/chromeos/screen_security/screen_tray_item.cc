// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/screen_security/screen_tray_item.h"

#include "ash/shelf/shelf_util.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {
const int kStopButtonRightPadding = 18;
}  // namespace

namespace ash {
namespace tray {

// ScreenTrayView implementations.
ScreenTrayView::ScreenTrayView(ScreenTrayItem* screen_tray_item, int icon_id)
    : TrayItemView(screen_tray_item),
      screen_tray_item_(screen_tray_item) {
  CreateImageView();
  image_view()->SetImage(ui::ResourceBundle::GetSharedInstance()
      .GetImageNamed(icon_id).ToImageSkia());

  Update();
}

ScreenTrayView::~ScreenTrayView() {
}

void ScreenTrayView::Update() {
  SetVisible(screen_tray_item_->is_started());
}


// ScreenStatusView implementations.
ScreenStatusView::ScreenStatusView(ScreenTrayItem* screen_tray_item,
                                   int icon_id,
                                   const base::string16& label_text,
                                   const base::string16& stop_button_text)
    : screen_tray_item_(screen_tray_item),
      icon_(NULL),
      label_(NULL),
      stop_button_(NULL),
      icon_id_(icon_id),
      label_text_(label_text),
      stop_button_text_(stop_button_text) {
  CreateItems();
  Update();
}

ScreenStatusView::~ScreenStatusView() {
}

void ScreenStatusView::Layout() {
  views::View::Layout();

  // Give the stop button the space it requests.
  gfx::Size stop_size = stop_button_->GetPreferredSize();
  gfx::Rect stop_bounds(stop_size);
  stop_bounds.set_x(width() - stop_size.width() - kStopButtonRightPadding);
  stop_bounds.set_y((height() - stop_size.height()) / 2);
  stop_button_->SetBoundsRect(stop_bounds);

  // Adjust the label's bounds in case it got cut off by |stop_button_|.
  if (label_->bounds().Intersects(stop_button_->bounds())) {
    gfx::Rect label_bounds = label_->bounds();
    label_bounds.set_width(
        stop_button_->x() - kTrayPopupPaddingBetweenItems - label_->x());
    label_->SetBoundsRect(label_bounds);
  }
}

void ScreenStatusView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == stop_button_);
  screen_tray_item_->Stop();
}

void ScreenStatusView::CreateItems() {
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        kTrayPopupPaddingHorizontal,
                                        0,
                                        kTrayPopupPaddingBetweenItems));
  icon_ = new FixedSizedImageView(0, kTrayPopupItemHeight);
  icon_->SetImage(bundle.GetImageNamed(icon_id_).ToImageSkia());
  AddChildView(icon_);
  label_ = new views::Label;
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
  label_->SetText(label_text_);
  AddChildView(label_);

  stop_button_ = new TrayPopupLabelButton(this, stop_button_text_);
  AddChildView(stop_button_);
}

void ScreenStatusView::Update() {
  // Hide the notification bubble when the ash tray bubble opens.
  screen_tray_item_->HideNotificationView();
  SetVisible(screen_tray_item_->is_started());
}

ScreenNotificationDelegate::ScreenNotificationDelegate(
    ScreenTrayItem* screen_tray)
  : screen_tray_(screen_tray) {
}

ScreenNotificationDelegate::~ScreenNotificationDelegate() {
}

void ScreenNotificationDelegate::ButtonClick(int button_index) {
  DCHECK_EQ(0, button_index);
  screen_tray_->Stop();
}

}  // namespace tray

ScreenTrayItem::ScreenTrayItem(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL),
      default_view_(NULL),
      is_started_(false),
      stop_callback_(base::Bind(&base::DoNothing)) {
}

ScreenTrayItem::~ScreenTrayItem() {}

void ScreenTrayItem::Update() {
  if (tray_view_)
    tray_view_->Update();
  if (default_view_)
    default_view_->Update();
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
    default_view_->Update();

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

void ScreenTrayItem::DestroyTrayView() {
  tray_view_ = NULL;
}

void ScreenTrayItem::DestroyDefaultView() {
  default_view_ = NULL;
}

void ScreenTrayItem::UpdateAfterShelfAlignmentChange(
    wm::ShelfAlignment alignment) {
  if (!tray_view_)
    return;

  // Center the item dependent on the orientation of the shelf.
  views::BoxLayout::Orientation layout = IsHorizontalAlignment(alignment)
                                             ? views::BoxLayout::kHorizontal
                                             : views::BoxLayout::kVertical;
  tray_view_->SetLayoutManager(new views::BoxLayout(layout, 0, 0, 0));
  tray_view_->Layout();
}

}  // namespace ash
