// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/shutdown_dialog.h"

#include "athena/screen/public/screen_manager.h"
#include "athena/strings/grit/athena_strings.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace athena {
namespace {

// The amount of time that the power button must be held to shut down the
// device after shutdown dialog is shown.
const int kShutdownTimeoutMs = 4000;

class ModalWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit ModalWidgetDelegate(views::View* contents_view)
      : contents_view_(contents_view) {}
  ~ModalWidgetDelegate() override {}

  // Overridden from WidgetDelegate:
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return contents_view_->GetWidget(); }
  const views::Widget* GetWidget() const override {
    return contents_view_->GetWidget();
  }
  views::View* GetContentsView() override { return contents_view_; }
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_SYSTEM; }

 private:
  views::View* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(ModalWidgetDelegate);
};

}  // namespace

ShutdownDialog::ShutdownDialog() : state_(STATE_OTHER) {
  InputManager::Get()->AddPowerButtonObserver(this);
}

ShutdownDialog::~ShutdownDialog() {
  InputManager::Get()->RemovePowerButtonObserver(this);
}

void ShutdownDialog::ShowShutdownWarningDialog() {
  state_ = STATE_SHUTDOWN_WARNING_VISIBLE;

  views::Label* label =
      new views::Label(l10n_util::GetStringUTF16(IDS_ATHENA_SHUTDOWN_WARNING));
  label->SetBackgroundColor(SK_ColorWHITE);
  label->SetFontList(gfx::FontList().DeriveWithStyle(gfx::Font::BOLD));

  views::View* container = new views::View;
  container->AddChildView(label);

  const int kBorderSpacing = 50;
  container->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, kBorderSpacing, kBorderSpacing, 0));
  container->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
  container->SetBorder(views::Border::CreateSolidBorder(1, SK_ColorBLACK));

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.delegate = new ModalWidgetDelegate(container);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = ScreenManager::Get()->GetContext();
  // Use top most modal container.
  params.keep_on_top = true;

  shutdown_warning_message_.reset(new views::Widget);
  shutdown_warning_message_->Init(params);
  shutdown_warning_message_->Show();
  shutdown_warning_message_->CenterWindow(container->GetPreferredSize());

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kShutdownTimeoutMs),
               this,
               &ShutdownDialog::Shutdown);
}

void ShutdownDialog::Shutdown() {
  state_ = STATE_SHUTDOWN_REQUESTED;
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->RequestShutdown();
}

void ShutdownDialog::OnPowerButtonStateChanged(
    PowerButtonObserver::State state) {
  if (state_ == STATE_SHUTDOWN_REQUESTED)
    return;

  switch (state) {
    case PRESSED:
      state_ = STATE_SUSPEND_ON_RELEASE;
      break;
    case LONG_PRESSED:
      ShowShutdownWarningDialog();
      break;
    case RELEASED:
      if (state_ == STATE_SUSPEND_ON_RELEASE) {
        chromeos::DBusThreadManager::Get()
            ->GetPowerManagerClient()
            ->RequestSuspend();
      }
      state_ = STATE_OTHER;
      timer_.Stop();
      shutdown_warning_message_.reset();
      break;
  }
}

}  // namespace athena
