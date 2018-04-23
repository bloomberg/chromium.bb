// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_installer_view.h"

#include <memory>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/account_id/account_id.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

using crostini::ConciergeClientResult;

namespace {
CrostiniInstallerView* g_crostini_installer_view = nullptr;

// The size of the download for the VM image.
// TODO(timloh): This is just a placeholder.
constexpr int kDownloadSizeInBytes = 200 * 1024 * 1024;

// TODO(timloh): We should get this from a ChromeLayoutProvider
constexpr int kDialogWidth = 448;

}  // namespace

void CrostiniInstallerView::Show(Profile* profile) {
  DCHECK(IsExperimentalCrostiniUIAvailable());
  if (!g_crostini_installer_view) {
    g_crostini_installer_view = new CrostiniInstallerView(profile);
    views::DialogDelegate::CreateDialogWidget(g_crostini_installer_view,
                                              nullptr, nullptr);
  }
  g_crostini_installer_view->GetWidget()->Show();
}

int CrostiniInstallerView::GetDialogButtons() const {
  if (state_ == State::PROMPT) {
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  }
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 CrostiniInstallerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALL_BUTTON);
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  if (state_ == State::ERROR || state_ == State::INSTALL_END)
    return l10n_util::GetStringUTF16(IDS_APP_CLOSE);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

base::string16 CrostiniInstallerView::GetWindowTitle() const {
  if (state_ == State::PROMPT) {
    const base::string16 device_type = ui::GetChromeOSDeviceName();
    return l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_TITLE, app_name_,
                                      device_type);
  }
  if (state_ == State::ERROR) {
    return l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_ERROR_TITLE,
                                      app_name_);
  }
  if (state_ == State::INSTALL_END) {
    return l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_COMPLETE,
                                      app_name_);
  }
  return l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_INSTALLING,
                                    app_name_);
}

bool CrostiniInstallerView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniInstallerView::Accept() {
  DCHECK_EQ(state_, State::PROMPT);
  state_ = State::INSTALL_START;
  DialogModelChanged();
  GetWidget()->UpdateWindowTitle();

  message_label_->SetVisible(false);

  progress_bar_ = new views::ProgressBar();
  AddChildView(progress_bar_);

  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());

  // Kick off the Crostini Restart sequence. We will be added as an observer.
  restart_id_ = crostini::CrostiniManager::GetInstance()->RestartCrostini(
      profile_, kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniInstallerView::StartContainerFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      this);
  return false;
}

bool CrostiniInstallerView::Cancel() {
  if (restart_id_ != crostini::CrostiniManager::kUninitializedRestartId) {
    // Abort the long-running flow, and prevent our RestartObserver methods
    // being called after "this" has been destroyed.
    crostini::CrostiniManager::GetInstance()->AbortRestartCrostini(restart_id_);
  }
  return true;  // Should close the dialog
}

gfx::Size CrostiniInstallerView::CalculatePreferredSize() const {
  int height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

// static
CrostiniInstallerView* CrostiniInstallerView::GetActiveViewForTesting() {
  return g_crostini_installer_view;
}

CrostiniInstallerView::CrostiniInstallerView(Profile* profile)
    : app_name_(base::ASCIIToUTF16(kCrostiniTerminalAppName)),
      profile_(profile),
      weak_ptr_factory_(this) {

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::TEXT));

  // TODO(timloh): Descenders in the message appear to be clipped, re-visit once
  // the UI has been fleshed out more.
  const base::string16 device_type = ui::GetChromeOSDeviceName();
  const base::string16 message = l10n_util::GetStringFUTF16(
      IDS_CROSTINI_INSTALLER_BODY, device_type, app_name_,
      ui::FormatBytesWithUnits(kDownloadSizeInBytes, ui::DATA_UNITS_MEBIBYTE,
                               /*show_units=*/true));
  message_label_ = new views::Label(message);
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label_);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_INSTALLER);
}

CrostiniInstallerView::~CrostiniInstallerView() {
  g_crostini_installer_view = nullptr;
}

void CrostiniInstallerView::StepProgress() {
  constexpr float kBase = static_cast<float>(State::INSTALL_START);
  constexpr float kNumSteps = static_cast<float>(State::INSTALL_END) - kBase;

  if (State::INSTALL_START < state_ && state_ <= State::INSTALL_END) {
    progress_bar_->SetValue((static_cast<float>(state_) - kBase) / kNumSteps);
  }

  DialogModelChanged();
}

void CrostiniInstallerView::HandleError(const base::string16& error_message) {
  state_ = State::ERROR;
  message_label_->SetVisible(true);
  message_label_->SetText(error_message);
  progress_bar_->SetVisible(false);
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  GetWidget()->UpdateWindowTitle();
}

void CrostiniInstallerView::OnComponentLoaded(ConciergeClientResult result) {
  DCHECK_EQ(state_, State::INSTALL_START);
  state_ = State::INSTALL_IMAGE_LOADER;
  if (result != ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to install the cros-termina component";
    HandleError(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_LOAD_TERMINA_ERROR));
    return;
  }
  VLOG(1) << "cros-termina install success";
  StepProgress();
}

void CrostiniInstallerView::OnConciergeStarted(ConciergeClientResult result) {
  DCHECK_EQ(state_, State::INSTALL_IMAGE_LOADER);
  state_ = State::START_CONCIERGE;
  if (result != ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to install start Concierge with error code: "
               << static_cast<int>(result);
    HandleError(l10n_util::GetStringUTF16(
        IDS_CROSTINI_INSTALLER_START_CONCIERGE_ERROR));
    return;
  }
  VLOG(1) << "VmConcierge service started";
  StepProgress();
}

void CrostiniInstallerView::OnDiskImageCreated(ConciergeClientResult result) {
  DCHECK_EQ(state_, State::INSTALL_IMAGE_LOADER);
  state_ = State::CREATE_DISK_IMAGE;
  if (result != ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to create disk imagewith error code: "
               << static_cast<int>(result);
    HandleError(l10n_util::GetStringUTF16(
        IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_ERROR));
    return;
  }
  VLOG(1) << "Created crostini disk image";
  StepProgress();
}

void CrostiniInstallerView::OnVmStarted(ConciergeClientResult result) {
  DCHECK_EQ(state_, State::CREATE_DISK_IMAGE);
  state_ = State::START_TERMINA_VM;
  if (result != ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to start Termina VM with error code: "
               << static_cast<int>(result);
    HandleError(l10n_util::GetStringUTF16(
        IDS_CROSTINI_INSTALLER_START_TERMINA_VM_ERROR));
    return;
  }
  VLOG(1) << "Started Termina VM successfully";
  StepProgress();
}

void CrostiniInstallerView::StartContainerFinished(
    ConciergeClientResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to start container with error code: "
               << static_cast<int>(result);
    HandleError(l10n_util::GetStringUTF16(
        IDS_CROSTINI_INSTALLER_START_CONTAINER_ERROR));
    return;
  }
  StepProgress();
  ShowLoginShell();
}

void CrostiniInstallerView::ShowLoginShell() {
  DCHECK_EQ(state_, State::START_CONTAINER);
  state_ = State::SHOW_LOGIN_SHELL;

  crostini::CrostiniManager::GetInstance()->LaunchContainerTerminal(
      profile_, kCrostiniDefaultVmName, kCrostiniDefaultContainerName);

  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  GetWidget()->UpdateWindowTitle();
  StepProgress();
  GetWidget()->Show();
}
