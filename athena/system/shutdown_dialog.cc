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

namespace athena {
namespace {

// The amount of time that the power button must be held to shut down the
// device after shutdown dialog is shown.
const int kShutdownTimeoutMs = 4000;

}  // namespace

ShutdownDialog::ShutdownDialog(aura::Window* dialog_container)
    : warning_message_container_(dialog_container), state_(STATE_OTHER) {
  InputManager::Get()->AddPowerButtonObserver(this);
}

ShutdownDialog::~ShutdownDialog() {
  InputManager::Get()->RemovePowerButtonObserver(this);
}

void ShutdownDialog::ShowShutdownWarningDialog() {
  state_ = STATE_SHUTDOWN_WARNING_VISIBLE;

  shutdown_warning_message_.reset(new views::Widget);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = warning_message_container_;
  shutdown_warning_message_->Init(params);

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

  shutdown_warning_message_->SetContentsView(container);
  shutdown_warning_message_->CenterWindow(container->GetPreferredSize());
  shutdown_warning_message_->Show();
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
