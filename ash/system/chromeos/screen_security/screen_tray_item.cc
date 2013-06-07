// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/screen_security/screen_tray_item.h"

#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {
const int kStopButtonRightPaddingDefaultView = 18;
const int kStopButtonRightPaddingNotificationView = 0;
const int kStopButtonLeftPaddingNotificationView = 4;
const int kVerticalPaddingScreenCaptureNotitification = 12;
}  // namespace

namespace ash {
namespace internal {

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
                                   ViewType view_type,
                                   int icon_id,
                                   const base::string16& label_text,
                                   const base::string16& stop_button_text)
    : screen_tray_item_(screen_tray_item),
      icon_(NULL),
      label_(NULL),
      stop_button_(NULL),
      view_type_(view_type),
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

  int stop_button_right_padding =
      view_type_ == VIEW_DEFAULT ?
          kStopButtonRightPaddingDefaultView :
          kStopButtonRightPaddingNotificationView;
  int stop_button_left_padding =
      view_type_ == VIEW_DEFAULT ?
          kTrayPopupPaddingBetweenItems :
          kStopButtonLeftPaddingNotificationView;
  // Give the stop button the space it requests.
  gfx::Size stop_size = stop_button_->GetPreferredSize();
  gfx::Rect stop_bounds(stop_size);
  stop_bounds.set_x(width() - stop_size.width() - stop_button_right_padding);
  stop_bounds.set_y((height() - stop_size.height()) / 2);
  stop_button_->SetBoundsRect(stop_bounds);

  // Adjust the label's bounds in case it got cut off by |stop_button_|.
  if (label_->bounds().Intersects(stop_button_->bounds())) {
    gfx::Rect label_bounds = label_->bounds();
    label_bounds.set_width(
        stop_button_->x() - stop_button_left_padding - label_->x());
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
  if (view_type_ == VIEW_DEFAULT) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                          kTrayPopupPaddingHorizontal,
                                          0,
                                          kTrayPopupPaddingBetweenItems));
    icon_ = new FixedSizedImageView(0, kTrayPopupItemHeight);
    icon_->SetImage(bundle.GetImageNamed(icon_id_).ToImageSkia());
    AddChildView(icon_);
  } else {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal,
                             0,
                             kVerticalPaddingScreenCaptureNotitification,
                             0));
  }

  label_ = new views::Label;
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
  label_->SetText(label_text_);
  AddChildView(label_);

  stop_button_ = new TrayPopupLabelButton(this, stop_button_text_);
  AddChildView(stop_button_);
}

void ScreenStatusView::Update() {
  if (view_type_ == VIEW_DEFAULT) {
    // Hide the notification bubble when the ash tray bubble opens.
    screen_tray_item_->HideNotificationView();
    SetVisible(screen_tray_item_->is_started());
  }
}


// ScreenNotificationView implementations.
ScreenNotificationView::ScreenNotificationView(
    ScreenTrayItem* screen_tray_item,
    int icon_id,
    const base::string16& label_text,
    const base::string16& stop_button_text)
        : TrayNotificationView(screen_tray_item, icon_id),
          screen_tray_item_(screen_tray_item) {
  screen_status_view_ = new ScreenStatusView(
      screen_tray_item,
      ScreenStatusView::VIEW_NOTIFICATION,
      icon_id,
      label_text,
      stop_button_text);
  InitView(screen_status_view_);
  Update();
}

ScreenNotificationView::~ScreenNotificationView() {
}

void ScreenNotificationView::Update() {
  if (screen_tray_item_->is_started())
    screen_status_view_->Update();
  else
    screen_tray_item_->HideNotificationView();
}

}  // namespace tray

ScreenTrayItem::ScreenTrayItem(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL),
      default_view_(NULL),
      notification_view_(NULL),
      is_started_(false),
      stop_callback_(base::Bind(&base::DoNothing)) {
}

ScreenTrayItem::~ScreenTrayItem() {}

void ScreenTrayItem::Update() {
  if (tray_view_)
    tray_view_->Update();
  if (default_view_)
    default_view_->Update();
  if (notification_view_)
    notification_view_->Update();
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
    ShowNotificationView();
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

void ScreenTrayItem::DestroyNotificationView() {
  notification_view_ = NULL;
}

}  // namespace internal
}  // namespace ash
