// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/kiosk_external_update_notification.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace chromeos {

namespace {

const SkColor kTextColor = SK_ColorBLACK;
const SkColor kWindowBackgroundColor = SK_ColorWHITE;
const int kWindowCornerRadius = 4;
const int kPreferredWidth = 600;
const int kPreferredHeight = 250;

}  // namespace

class KioskExternalUpdateNotificationView : public views::WidgetDelegateView {
 public:
  explicit KioskExternalUpdateNotificationView(
      KioskExternalUpdateNotification* owner)
      : owner_(owner), widget_closed_(false) {
    AddLabel();
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  ~KioskExternalUpdateNotificationView() override {
    widget_closed_ = true;
    InformOwnerForDismiss();
  }

  // Closes the widget immediately from |owner_|.
  void CloseByOwner() {
    owner_ = NULL;
    if (!widget_closed_) {
      widget_closed_ = true;
      GetWidget()->Close();
    }
  }

  void SetMessage(const base::string16& message) { label_->SetText(message); }

  // views::WidgetDelegateView overrides:
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(kWindowBackgroundColor);
    canvas->DrawRoundRect(GetLocalBounds(), kWindowCornerRadius, flags);
    views::WidgetDelegateView::OnPaint(canvas);
  }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kPreferredWidth, kPreferredHeight);
  }

 private:
  void AddLabel() {
    label_ = new views::Label;
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    label_->SetFontList(rb->GetFontList(ui::ResourceBundle::BoldFont));
    label_->SetEnabledColor(kTextColor);
    label_->SetAutoColorReadabilityEnabled(false);
    label_->SetMultiLine(true);
    AddChildView(label_);
  }

  void InformOwnerForDismiss() {
    // Inform the |owner_| that we are going away.
    if (owner_) {
      KioskExternalUpdateNotification* owner = owner_;
      owner_ = NULL;
      owner->Dismiss();
    }
  }

  // The owner of this message which needs to get notified when the message
  // closes.
  KioskExternalUpdateNotification* owner_;
  views::Label* label_;  // owned by views hierarchy.

  // True if the widget got already closed.
  bool widget_closed_;

  DISALLOW_COPY_AND_ASSIGN(KioskExternalUpdateNotificationView);
};

KioskExternalUpdateNotification::KioskExternalUpdateNotification(
    const base::string16& message) {
  CreateAndShowNotificationView(message);
}

KioskExternalUpdateNotification::~KioskExternalUpdateNotification() {
  Dismiss();
}

void KioskExternalUpdateNotification::ShowMessage(
    const base::string16& message) {
  if (view_)
    view_->SetMessage(message);
}

void KioskExternalUpdateNotification::CreateAndShowNotificationView(
    const base::string16& message) {
  view_ = new KioskExternalUpdateNotificationView(this);
  view_->SetMessage(message);

  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  gfx::Size view_size = view_->GetPreferredSize();
  gfx::Rect bounds((display_size.width() - view_size.width()) / 2,
                   (display_size.height() - view_size.height()) / 10,
                   view_size.width(), view_size.height());
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.accept_events = false;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.delegate = view_;
  params.bounds = bounds;
  // The notification is shown on the primary display.
  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_SettingBubbleContainer);
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  gfx::NativeView native_view = widget->GetNativeView();
  native_view->SetName("KioskExternalUpdateNotification");
  widget->Show();
}

void KioskExternalUpdateNotification::Dismiss() {
  if (view_) {
    KioskExternalUpdateNotificationView* view = view_;
    view_ = NULL;
    view->CloseByOwner();
  }
}

}  // namespace chromeos
