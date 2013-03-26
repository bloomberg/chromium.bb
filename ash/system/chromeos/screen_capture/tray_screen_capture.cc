// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/screen_capture/tray_screen_capture.h"

#include "ash/shell.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_popup_label_button.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
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

class ScreenCaptureTrayView : public TrayItemView {
 public:
  explicit ScreenCaptureTrayView(TrayScreenCapture* tray_screen_capture)
      : TrayItemView(tray_screen_capture),
        tray_screen_capture_(tray_screen_capture) {
    CreateImageView();
    image_view()->SetImage(ui::ResourceBundle::GetSharedInstance().
        GetImageNamed(IDR_AURA_UBER_TRAY_DISPLAY_LIGHT).ToImageSkia());

    Update();
  }
  virtual ~ScreenCaptureTrayView() {
  }

  void Update() {
    SetVisible(tray_screen_capture_->screen_capture_on());
  }

 private:
  TrayScreenCapture* tray_screen_capture_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureTrayView);
};

class ScreenCaptureStatusView : public views::View,
                                public views::ButtonListener {
 public:
  enum ViewType {
      VIEW_DEFAULT,
      VIEW_NOTIFICATION
  };

  explicit ScreenCaptureStatusView(TrayScreenCapture* tray_screen_capture,
                                   ViewType view_type)
      : icon_(NULL),
        label_(NULL),
        stop_button_(NULL),
        tray_screen_capture_(tray_screen_capture),
        view_type_(view_type) {
    CreateItems();
    Update();
  }

  virtual ~ScreenCaptureStatusView() {
  }

  // Overridden from views::View.
  virtual void Layout() OVERRIDE {
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

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    DCHECK(sender == stop_button_);
    tray_screen_capture_->StopScreenCapture();
  }

  void CreateItems() {
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    if (view_type_ == VIEW_DEFAULT) {
      SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                            kTrayPopupPaddingHorizontal,
                                            0,
                                            kTrayPopupPaddingBetweenItems));
      icon_ = new FixedSizedImageView(0, kTrayPopupItemHeight);
      icon_->SetImage(
          bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DISPLAY).ToImageSkia());
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
    label_->SetText(tray_screen_capture_->screen_capture_status());
    AddChildView(label_);

    stop_button_ = new TrayPopupLabelButton(
        this,
        bundle.GetLocalizedString(
            IDS_CHROMEOS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP));
    AddChildView(stop_button_);
  }

  void Update() {
    if (view_type_ == VIEW_DEFAULT) {
      // Hide the notification bubble when the ash tray bubble opens.
      tray_screen_capture_->HideNotificationView();
      SetVisible(tray_screen_capture_->screen_capture_on());
    }
  }

 private:
  views::ImageView* icon_;
  views::Label* label_;
  TrayPopupLabelButton* stop_button_;
  TrayScreenCapture* tray_screen_capture_;
  ViewType view_type_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureStatusView);
};

class ScreenCaptureNotificationView : public TrayNotificationView {
 public:
  explicit ScreenCaptureNotificationView(TrayScreenCapture* tray_screen_capture)
      : TrayNotificationView(tray_screen_capture, IDR_AURA_UBER_TRAY_DISPLAY),
        tray_screen_capture_(tray_screen_capture) {
    screen_capture_status_view_ =
        new ScreenCaptureStatusView(
            tray_screen_capture, ScreenCaptureStatusView::VIEW_NOTIFICATION);
    InitView(screen_capture_status_view_);
  }

  virtual ~ScreenCaptureNotificationView() {
  }

  void Update() {
    if (tray_screen_capture_->screen_capture_on())
      screen_capture_status_view_->Update();
    else
      tray_screen_capture_->HideNotificationView();
  }

 private:
  TrayScreenCapture* tray_screen_capture_;
  ScreenCaptureStatusView* screen_capture_status_view_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationView);
};

}  // namespace tray

TrayScreenCapture::TrayScreenCapture(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_(NULL),
      default_(NULL),
      notification_(NULL),
      screen_capture_on_(false),
      stop_callback_(base::Bind(&base::DoNothing)) {
  Shell::GetInstance()->system_tray_notifier()->
      AddScreenCaptureObserver(this);
}

TrayScreenCapture::~TrayScreenCapture() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveScreenCaptureObserver(this);
}

void TrayScreenCapture::Update() {
  if (tray_)
    tray_->Update();
  if (default_)
    default_->Update();
  if (notification_)
    notification_->Update();
}

views::View* TrayScreenCapture::CreateTrayView(user::LoginStatus status) {
  tray_ = new tray::ScreenCaptureTrayView(this);
  return tray_;
}

views::View* TrayScreenCapture::CreateDefaultView(user::LoginStatus status) {
  default_ = new tray::ScreenCaptureStatusView(
      this, tray::ScreenCaptureStatusView::VIEW_DEFAULT);
  return default_;
}

views::View* TrayScreenCapture::CreateNotificationView(
    user::LoginStatus status) {
  notification_ = new tray::ScreenCaptureNotificationView(this);
  return notification_;
}

void TrayScreenCapture::DestroyTrayView() {
  tray_ = NULL;
}

void TrayScreenCapture::DestroyDefaultView() {
  default_ = NULL;
}

void TrayScreenCapture::DestroyNotificationView() {
  notification_ = NULL;
}

void TrayScreenCapture::OnScreenCaptureStart(
    const base::Closure& stop_callback,
    const string16& screen_capture_status) {
  stop_callback_ = stop_callback;
  screen_capture_status_ = screen_capture_status;
  set_screen_capture_on(true);

  if (tray_)
    tray_->Update();

  if (default_)
    default_->Update();

  if (!system_tray()->HasSystemBubbleType(
      SystemTrayBubble::BUBBLE_TYPE_DEFAULT)) {
    ShowNotificationView();
  }
}

void TrayScreenCapture::OnScreenCaptureStop() {
  set_screen_capture_on(false);
  Update();
}

void TrayScreenCapture::StopScreenCapture() {
  if (stop_callback_.is_null())
    return;

  base::Closure callback = stop_callback_;
  stop_callback_.Reset();
  callback.Run();
}

}  // namespace internal
}  // namespace ash
