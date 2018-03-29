// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_installer_view.h"

#include <memory>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"
#include "chrome/browser/ui/app_list/crostini/crostini_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/account_id/account_id.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
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

namespace {
CrostiniInstallerView* g_crostini_installer_view = nullptr;

// The size of the download for the VM image.
// TODO(timloh): This is just a placeholder.
constexpr int kDownloadSizeInBytes = 200 * 1024 * 1024;

// TODO(timloh): We should get this from a ChromeLayoutProvider
constexpr int kDialogWidth = 448;

}  // namespace

void CrostiniInstallerView::Show(const CrostiniAppItem* app_item,
                                 Profile* profile) {
  DCHECK(IsExperimentalCrostiniUIAvailable());
  if (!g_crostini_installer_view) {
    g_crostini_installer_view = new CrostiniInstallerView(app_item, profile);
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

  InstallImageLoader();
  return false;
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

CrostiniInstallerView::CrostiniInstallerView(const CrostiniAppItem* app_item,
                                             Profile* profile)
    : app_name_(base::ASCIIToUTF16(app_item->name())),
      profile_(profile),
      cryptohome_id_(chromeos::ProfileHelper::Get()
                         ->GetUserByProfile(profile)
                         ->username_hash()),
      weak_ptr_factory_(this) {
  // Get rid of the @domain.name in the container_user_name_ (an email address).
  container_user_name_ = profile_->GetProfileUserName();
  container_user_name_.substr(0, container_user_name_.find('@'));

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

void CrostiniInstallerView::StepProgressBar() {
  constexpr float kBase = static_cast<float>(State::INSTALL_START);
  constexpr float kNumSteps = static_cast<float>(State::INSTALL_END) - kBase;

  if (State::INSTALL_START < state_ && state_ <= State::INSTALL_END) {
    progress_bar_->SetValue((static_cast<float>(state_) - kBase) / kNumSteps);
  }
}

void CrostiniInstallerView::HandleError(const base::string16& error_message) {
  state_ = State::ERROR;
  message_label_->SetVisible(true);
  message_label_->SetText(error_message);
  progress_bar_->SetVisible(false);
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  GetWidget()->UpdateWindowTitle();
}

void CrostiniInstallerView::InstallImageLoader() {
  VLOG(1) << "Installing cros-termina component";
  DCHECK_EQ(state_, State::INSTALL_START);
  state_ = State::INSTALL_IMAGE_LOADER;
  g_browser_process->platform_part()->cros_component_manager()->Load(
      "cros-termina",
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      base::BindOnce(InstallImageLoaderFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerView::InstallImageLoaderFinished(
    base::WeakPtr<CrostiniInstallerView> weak_ptr,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& result) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &CrostiniInstallerView::InstallImageLoaderFinishedOnUIThread,
          weak_ptr, error, result));
}

void CrostiniInstallerView::InstallImageLoaderFinishedOnUIThread(
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& result) {
  if (error != component_updater::CrOSComponentManager::Error::NONE) {
    LOG(ERROR)
        << "Failed to install the cros-termina component with error code: "
        << static_cast<int>(error);
    HandleError(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_LOAD_TERMINA_ERROR));
    return;
  }
  VLOG(1) << "cros-termina install success";
  StepProgressBar();
  StartVmConcierge();
}

void CrostiniInstallerView::StartVmConcierge() {
  DCHECK_EQ(state_, State::INSTALL_IMAGE_LOADER);
  state_ = State::START_CONCIERGE;

  crostini::CrostiniManager::GetInstance()->StartVmConcierge(
      base::BindOnce(&CrostiniInstallerView::StartVmConciergeFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerView::StartVmConciergeFinished(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    LOG(ERROR) << "Failed to install start Concierge";
    HandleError(l10n_util::GetStringUTF16(
        IDS_CROSTINI_INSTALLER_START_CONCIERGE_ERROR));
    return;
  }
  VLOG(1) << "VmConcierge service started";
  StepProgressBar();
  CreateDiskImage();
}

void CrostiniInstallerView::CreateDiskImage() {
  DCHECK_EQ(state_, State::START_CONCIERGE);
  state_ = State::CREATE_DISK_IMAGE;

  VLOG(1) << "Creating crostini disk image";
  crostini::CrostiniManager::GetInstance()->CreateDiskImage(
      cryptohome_id_, base::FilePath(kCrostiniDefaultVmName),
      vm_tools::concierge::StorageLocation::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniInstallerView::CreateDiskImageFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerView::CreateDiskImageFinished(
    crostini::ConciergeClientResult result,
    const base::FilePath& result_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != crostini::ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to create disk image with error code: "
               << static_cast<int>(result);
    HandleError(l10n_util::GetStringUTF16(
        IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_ERROR));
    return;
  }
  VLOG(1) << "Created crostini disk image";
  StepProgressBar();
  StartTerminaVm(result_path);
}

void CrostiniInstallerView::StartTerminaVm(const base::FilePath& result_path) {
  DCHECK_EQ(state_, State::CREATE_DISK_IMAGE);
  state_ = State::START_TERMINA_VM;

  VLOG(1) << "Starting Termina VM";
  crostini::CrostiniManager::GetInstance()->StartTerminaVm(
      kCrostiniDefaultVmName, result_path,
      base::BindOnce(&CrostiniInstallerView::StartTerminaVmFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerView::StartTerminaVmFinished(
    crostini::ConciergeClientResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != crostini::ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to start Termina VM with error code: "
               << static_cast<int>(result);
    HandleError(l10n_util::GetStringUTF16(
        IDS_CROSTINI_INSTALLER_START_TERMINA_VM_ERROR));
    return;
  }
  VLOG(1) << "Started Termina VM successfully";
  StepProgressBar();
  StartContainer();
}

void CrostiniInstallerView::StartContainer() {
  DCHECK_EQ(state_, State::START_TERMINA_VM);
  state_ = State::START_CONTAINER;
  VLOG(1) << "In vm " << kCrostiniDefaultVmName << ", starting container "
          << kCrostiniDefaultContainerName << " as user "
          << container_user_name_;
  crostini::CrostiniManager::GetInstance()->StartContainer(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      container_user_name_,
      base::BindOnce(&CrostiniInstallerView::StartContainerFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerView::StartContainerFinished(
    crostini::ConciergeClientResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != crostini::ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to start container with error code: "
               << static_cast<int>(result);
    HandleError(l10n_util::GetStringUTF16(
        IDS_CROSTINI_INSTALLER_START_CONTAINER_ERROR));
    return;
  }
  StepProgressBar();
  ShowLoginShell();
}

void CrostiniInstallerView::ShowLoginShell() {
  DCHECK_EQ(state_, State::START_CONTAINER);
  state_ = State::SHOW_LOGIN_SHELL;

  std::string vsh_crosh = base::StringPrintf(
      "chrome-extension://%s/html/crosh.html?command=vmshell",
      kCrostiniCroshBuiltinAppId);
  std::string vm_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--vm_name=%s", kCrostiniDefaultVmName), false);
  std::string lxd_dir =
      net::EscapeQueryParamValue("LXD_DIR=/mnt/stateful/lxd", false);
  std::string lxd_conf =
      net::EscapeQueryParamValue("LXD_CONF=/mnt/stateful/lxd_conf", false);

  std::vector<base::StringPiece> pieces = {vsh_crosh,
                                           vm_name_param,
                                           "--",
                                           lxd_dir,
                                           lxd_conf,
                                           "run_container.sh",
                                           "--container_name",
                                           kCrostiniDefaultContainerName,
                                           "--user",
                                           container_user_name_,
                                           "--shell"};

  GURL vsh_in_crosh_url(base::JoinString(pieces, "&args[]="));

  AppLaunchParams launch_params(profile_,
                                nullptr,  // this is a URL app.  No extension.
                                extensions::LAUNCH_CONTAINER_WINDOW,
                                WindowOpenDisposition::NEW_WINDOW,
                                extensions::SOURCE_APP_LAUNCHER);

  OpenApplicationWindow(launch_params, vsh_in_crosh_url);

  StepProgressBar();
  message_label_->SetVisible(true);
  message_label_->SetText(
      l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_COMPLETE));
  GetWidget()->Show();
}
