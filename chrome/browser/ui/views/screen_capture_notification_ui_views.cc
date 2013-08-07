// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/screen_capture_notification_ui.h"

#include "ash/shell.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/aura/root_window.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/corewm/shadow_types.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

const int kMinimumWidth = 460;
const int kMaximumWidth = 1000;
const int kHorizontalMargin = 10;
const int kPadding = 5;
const int kPaddingLeft = 10;

namespace {

// A ClientView that overrides NonClientHitTest() so that the whole window area
// acts as a window caption, except a rect specified using SetClientRect().
// ScreenCaptureNotificationUIViews uses this class to make the notification bar
// draggable.
class NotificationBarClientView : public views::ClientView {
 public:
  NotificationBarClientView(views::Widget* widget, views::View* view)
      : views::ClientView(widget, view) {
  }
  virtual ~NotificationBarClientView() {}

  void SetClientRect(const gfx::Rect& rect) {
    rect_ = rect;
  }

  // views::ClientView overrides.
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE  {
    if (!bounds().Contains(point))
      return HTNOWHERE;
    // The whole window is HTCAPTION, except the |rect_|.
    if (rect_.Contains(gfx::PointAtOffsetFromOrigin(point - bounds().origin())))
      return HTCLIENT;

    return HTCAPTION;
  }

 private:
  gfx::Rect rect_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBarClientView);
};

}  // namespace

// ScreenCaptureNotificationUI implementation using Views.
class ScreenCaptureNotificationUIViews
    : public ScreenCaptureNotificationUI,
      public views::WidgetDelegateView,
      public views::ButtonListener {
 public:
  explicit ScreenCaptureNotificationUIViews(const string16& text);
  virtual ~ScreenCaptureNotificationUIViews();

  // ScreenCaptureNotificationUI interface.
  virtual void OnStarted(const base::Closure& stop_callback) OVERRIDE;

  // views::View overrides.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  // views::WidgetDelegateView overrides.
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::ClientView* CreateClientView(views::Widget* widget) OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual bool ShouldShowCloseButton() const OVERRIDE;

  // views::ButtonListener interface.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  // Helper to call |stop_callback_|.
  void NotifyStopped();

  const string16 text_;
  base::Closure stop_callback_;
  NotificationBarClientView* client_view_;
  views::ImageView* gripper_;
  views::Label* label_;
  views::BlueButton* stop_button_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUIViews);
};

ScreenCaptureNotificationUIViews::ScreenCaptureNotificationUIViews(
    const string16& text)
    : text_(text),
      client_view_(NULL),
      gripper_(NULL),
      label_(NULL),
      stop_button_(NULL) {
  set_owned_by_client();

  set_background(views::Background::CreateSolidBackground(GetNativeTheme()->
      GetSystemColor(ui::NativeTheme::kColorId_DialogBackground)));

  gripper_ = new views::ImageView();
  gripper_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_SCREEN_CAPTURE_NOTIFICATION_GRIP));
  AddChildView(gripper_);

  label_ = new views::Label();
  AddChildView(label_);

  string16 stop_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP);
  stop_button_ = new views::BlueButton(this, stop_text);
  AddChildView(stop_button_);
}

ScreenCaptureNotificationUIViews::~ScreenCaptureNotificationUIViews() {
  stop_callback_.Reset();
  delete GetWidget();
}

void ScreenCaptureNotificationUIViews::OnStarted(
    const base::Closure& stop_callback) {
  stop_callback_ = stop_callback;

  label_->SetElideBehavior(views::Label::ELIDE_IN_MIDDLE);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetText(text_);

  views::Widget* widget = new views::Widget;

  views::Widget::InitParams params;
  params.delegate = this;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.remove_standard_frame = true;
  params.keep_on_top = true;
  params.top_level = true;
  params.can_activate = false;

#if defined(USE_ASH)
  // TODO(sergeyu): The notification bar must be shown on the monitor that's
  // being captured. Make sure it's always the case. Currently we always capture
  // the primary monitor.
  if (ash::Shell::HasInstance())
    params.context = ash::Shell::GetPrimaryRootWindow();
#endif

  widget->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);
  widget->Init(params);
  widget->SetAlwaysOnTop(true);

  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  // TODO(sergeyu): Move the notification to the display being captured when
  // per-display screen capture is supported.
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();

  // Place the bar in the center of the bottom of the display.
  gfx::Size size = widget->non_client_view()->GetPreferredSize();
  gfx::Rect bounds(
      work_area.x() + work_area.width() / 2 - size.width() / 2,
      work_area.y() + work_area.height() - size.height(),
      size.width(), size.height());
  widget->SetBounds(bounds);

  widget->Show();
}

gfx::Size ScreenCaptureNotificationUIViews::GetPreferredSize() {
  gfx::Size grip_size = gripper_->GetPreferredSize();
  gfx::Size label_size = child_at(1)->GetPreferredSize();
  gfx::Size button_size = child_at(2)->GetPreferredSize();
  int width = kHorizontalMargin * 2 + grip_size.width() + label_size.width() +
      button_size.width();
  width = std::max(width, kMinimumWidth);
  width = std::min(width, kMaximumWidth);
  return gfx::Size(width, std::max(label_size.height(), button_size.height()));
}

void ScreenCaptureNotificationUIViews::Layout() {
  gfx::Rect grip_rect(gripper_->GetPreferredSize());
  grip_rect.set_y(bounds().height() / 2 - grip_rect.height() / 2);
  gripper_->SetBoundsRect(grip_rect);

  gfx::Rect button_rect(stop_button_->GetPreferredSize());
  button_rect.set_x(bounds().width() - button_rect.width());
  stop_button_->SetBoundsRect(button_rect);

  gfx::Rect label_rect;
  label_rect.set_x(grip_rect.right() + kHorizontalMargin);
  label_rect.set_width(button_rect.x() - kHorizontalMargin - label_rect.x());
  label_rect.set_height(bounds().height());
  label_->SetBoundsRect(label_rect);

  client_view_->SetClientRect(button_rect);
}

void ScreenCaptureNotificationUIViews::DeleteDelegate() {
  NotifyStopped();
}

views::View* ScreenCaptureNotificationUIViews::GetContentsView() {
  return this;
}

views::ClientView* ScreenCaptureNotificationUIViews::CreateClientView(
    views::Widget* widget) {
  DCHECK(!client_view_);
  client_view_ = new NotificationBarClientView(widget, this);
  return client_view_;
}

views::NonClientFrameView*
ScreenCaptureNotificationUIViews::CreateNonClientFrameView(
    views::Widget* widget) {
  views::BubbleFrameView* frame = new views::BubbleFrameView(
      gfx::Insets(kPadding, kPaddingLeft, kPadding, kPadding));
  SkColor color = widget->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground);
  views::BubbleBorder* border = new views::BubbleBorder(
      views::BubbleBorder::NONE, views::BubbleBorder::SMALL_SHADOW, color);
  frame->SetBubbleBorder(border);
  return frame;
}

string16 ScreenCaptureNotificationUIViews::GetWindowTitle() const {
  return text_;
}

bool ScreenCaptureNotificationUIViews::ShouldShowWindowTitle() const {
  return false;
}

bool ScreenCaptureNotificationUIViews::ShouldShowCloseButton() const {
  return false;
}

void ScreenCaptureNotificationUIViews::ButtonPressed(views::Button* sender,
                                                     const ui::Event& event) {
  NotifyStopped();
}

void ScreenCaptureNotificationUIViews::NotifyStopped() {
  if (!stop_callback_.is_null()) {
    base::Closure callback = stop_callback_;
    stop_callback_.Reset();
    callback.Run();
  }
}

}  // namespace

scoped_ptr<ScreenCaptureNotificationUI> ScreenCaptureNotificationUI::Create(
    const string16& text) {
  return scoped_ptr<ScreenCaptureNotificationUI>(
      new ScreenCaptureNotificationUIViews(text));
}
