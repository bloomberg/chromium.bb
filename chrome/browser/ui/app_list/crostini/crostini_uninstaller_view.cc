// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_uninstaller_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {
CrostiniUninstallerView* g_crostini_uninstaller_view = nullptr;

// TODO(nverne): We should get this from a ChromeLayoutProvider
constexpr int kDialogWidth = 448;

}  // namespace

void CrostiniUninstallerView::Show(Profile* profile) {
  DCHECK(IsCrostiniUIAllowedForProfile(profile));
  if (!g_crostini_uninstaller_view) {
    g_crostini_uninstaller_view = new CrostiniUninstallerView(profile);
    views::DialogDelegate::CreateDialogWidget(g_crostini_uninstaller_view,
                                              nullptr, nullptr);
  }
  g_crostini_uninstaller_view->GetWidget()->Show();
}

int CrostiniUninstallerView::GetDialogButtons() const {
  switch (state_) {
    case State::PROMPT:
      return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
    case State::UNINSTALLING:
      return ui::DIALOG_BUTTON_NONE;
    case State::ERROR:
      return ui::DIALOG_BUTTON_CANCEL;
  }
  NOTREACHED();
  return 0;
}

base::string16 CrostiniUninstallerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_CROSTINI_UNINSTALLER_UNINSTALL_BUTTON);
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

base::string16 CrostiniUninstallerView::GetWindowTitle() const {
  const base::string16 device_type = ui::GetChromeOSDeviceName();
  return l10n_util::GetStringFUTF16(IDS_CROSTINI_UNINSTALLER_TITLE, app_name_,
                                    device_type);
}

bool CrostiniUninstallerView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniUninstallerView::Accept() {
  state_ = State::UNINSTALLING;
  message_label_->SetText(
      l10n_util::GetStringUTF16(IDS_CROSTINI_UNINSTALLER_UNINSTALLING_MESSAGE));

  // Kick off the Crostini Remove sequence.
  crostini::CrostiniManager::GetInstance()->RemoveCrostini(
      profile_, kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniUninstallerView::UninstallCrostiniFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  progress_bar_ = new views::ProgressBar();
  AddChildView(progress_bar_);
  // Setting value to -1 makes the progress bar play the
  // "indeterminate animation".
  progress_bar_->SetValue(-1);
  GetWidget()->UpdateWindowTitle();
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  DialogModelChanged();
  return false;  // Should not close the dialog
}

bool CrostiniUninstallerView::Cancel() {
  return true;  // Should close the dialog
}

gfx::Size CrostiniUninstallerView::CalculatePreferredSize() const {
  int height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

CrostiniUninstallerView::CrostiniUninstallerView(Profile* profile)
    : app_name_(base::ASCIIToUTF16(kCrostiniTerminalAppName)),
      profile_(profile),
      weak_ptr_factory_(this) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::TEXT));

  const base::string16 device_type = ui::GetChromeOSDeviceName();
  const base::string16 message = l10n_util::GetStringFUTF16(
      IDS_CROSTINI_UNINSTALLER_BODY, app_name_, device_type);
  message_label_ = new views::Label(message);
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label_);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_UNINSTALLER);
}

void CrostiniUninstallerView::HandleError(const base::string16& error_message) {
  state_ = State::ERROR;
  message_label_->SetVisible(true);
  message_label_->SetText(error_message);
  progress_bar_->SetVisible(false);
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  GetWidget()->UpdateWindowTitle();
}

void CrostiniUninstallerView::UninstallCrostiniFinished(
    crostini::ConciergeClientResult result) {
  if (result != crostini::ConciergeClientResult::SUCCESS) {
    HandleError(
        l10n_util::GetStringFUTF16(IDS_CROSTINI_UNINSTALLER_ERROR, app_name_));
    return;
  }
  GetWidget()->Close();
}

CrostiniUninstallerView::~CrostiniUninstallerView() {
  g_crostini_uninstaller_view = nullptr;
}
