// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/multi_user/user_switch_util.h"

#include "ash/shell.h"
#include "ash/system/chromeos/screen_security/screen_tray_item.h"
#include "ash/system/tray/system_tray.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {

// Default width/height of the dialog.
const int kDefaultWidth = 500;
const int kDefaultHeight = 150;

const int kPaddingToMessage = 30;
const int kInset = 40;
const int kTopInset = 10;

////////////////////////////////////////////////////////////////////////////////
// Dialog for multi-profiles desktop casting warning.
class DesktopCastingWarningView : public views::DialogDelegateView {
 public:
  DesktopCastingWarningView(base::Callback<void()> on_accept);
  virtual ~DesktopCastingWarningView();

  static void ShowDialog(const base::Callback<void()> on_accept);

  // views::DialogDelegate overrides.
  virtual bool Accept() OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
        ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;

  // views::WidgetDelegate overrides.
  virtual ui::ModalType GetModalType() const OVERRIDE;

  // views::View overrides.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

 private:
  void InitDialog();

  const base::Callback<void()> on_switch_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCastingWarningView);
};

// The current instance of the running dialog - or NULL. This is used for
// unittest related functions.
static DesktopCastingWarningView* instance_for_test;

////////////////////////////////////////////////////////////////////////////////
// DesktopCastingWarningView implementation.

DesktopCastingWarningView::DesktopCastingWarningView(
    const base::Callback<void()> on_switch)
    : on_switch_(on_switch) {
  DCHECK(!instance_for_test);
  instance_for_test = this;
}

DesktopCastingWarningView::~DesktopCastingWarningView() {
  DCHECK(instance_for_test);
  instance_for_test = NULL;
}

// static
void DesktopCastingWarningView::ShowDialog(
    const base::Callback<void()> on_accept) {
  DesktopCastingWarningView* dialog_view =
      new DesktopCastingWarningView(on_accept);
  views::DialogDelegate::CreateDialogWidget(
      dialog_view, ash::Shell::GetTargetRootWindow(), NULL);
  dialog_view->InitDialog();
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();
}

bool DesktopCastingWarningView::Accept() {
  // Stop screen sharing and capturing.
  SystemTray* system_tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  if (system_tray->GetScreenShareItem()->is_started())
    system_tray->GetScreenShareItem()->Stop();
  if (system_tray->GetScreenCaptureItem()->is_started())
    system_tray->GetScreenCaptureItem()->Stop();

  on_switch_.Run();
  return true;
}

base::string16 DesktopCastingWarningView::GetDialogButtonLabel(
      ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      button == ui::DIALOG_BUTTON_OK ?
          IDS_DESKTOP_CASTING_ACTIVE_BUTTON_SWITCH_USER :
          IDS_DESKTOP_CASTING_ACTIVE_BUTTON_ABORT_USER_SWITCH);
}

bool DesktopCastingWarningView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL;
}

int DesktopCastingWarningView::GetDefaultDialogButton() const {
  // The default should turn off the casting.
  return ui::DIALOG_BUTTON_CANCEL;
}

ui::ModalType DesktopCastingWarningView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

gfx::Size DesktopCastingWarningView::GetPreferredSize() const {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

void DesktopCastingWarningView::InitDialog() {
  const gfx::Insets kDialogInsets(kTopInset, kInset, kInset, kInset);

  // Create the views and layout manager and set them up.
  views::GridLayout* grid_layout = views::GridLayout::CreatePanel(this);
  grid_layout->SetInsets(kDialogInsets);

  views::ColumnSet* column_set = grid_layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  // Title
  views::Label* title_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_DESKTOP_CASTING_ACTIVE_TITLE));
  title_label_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumBoldFont));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(title_label_);
  grid_layout->AddPaddingRow(0, kPaddingToMessage);

  // Explanation string
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_DESKTOP_CASTING_ACTIVE_MESSAGE));
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(label);

  SetLayoutManager(grid_layout);
  Layout();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Factory function.

void TrySwitchingActiveUser(
   const base::Callback<void()> on_switch) {
  // Some unit tests do not have a shell. In that case simply execute.
  if (!ash::Shell::HasInstance()) {
    on_switch.Run();
    return;
  }
  // If neither screen sharing nor capturing is going on we can immediately
  // switch users.
  SystemTray* system_tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  if (!system_tray->GetScreenShareItem()->is_started() &&
      !system_tray->GetScreenCaptureItem()->is_started()) {
    on_switch.Run();
    return;
  }
  DesktopCastingWarningView::ShowDialog(on_switch);
}

bool TestAndTerminateDesktopCastingWarningForTest(bool accept) {
  if (!instance_for_test)
    return false;
  if (accept)
    instance_for_test->Accept();
  delete instance_for_test->GetWidget()->GetNativeWindow();
  CHECK(!instance_for_test);
  return true;
}

}  // namespace chromeos
