// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_installer_view.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_typography.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_client_view.h"

using crostini::ConciergeClientResult;

namespace {
CrostiniInstallerView* g_crostini_installer_view = nullptr;

// The size of the download for the VM image.
// TODO(timloh): This is just a placeholder.
constexpr int kDownloadSizeInBytes = 300 * 1024 * 1024;

constexpr int kOOBEButtonRowInset = 32;
constexpr int kOOBEWindowWidth = 768;
constexpr int kOOBEWindowHeight = 640 - 2 * kOOBEButtonRowInset;
constexpr int kLinuxIllustrationWidth = 448;
constexpr int kLinuxIllustrationHeight = 180;

constexpr char kCrostiniSetupResultHistogram[] = "Crostini.SetupResult";
constexpr char kCrostiniSetupSourceHistogram[] = "Crostini.SetupSource";
constexpr char kCrostiniTimeFromDeviceSetupToInstall[] =
    "Crostini.TimeFromDeviceSetupToInstall";

void RecordTimeFromDeviceSetupToInstallMetric() {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&chromeos::StartupUtils::GetTimeSinceOobeFlagFileCreation),
      base::BindOnce([](base::TimeDelta time_from_device_setup) {
        if (time_from_device_setup.is_zero())
          return;

        base::UmaHistogramCustomTimes(kCrostiniTimeFromDeviceSetupToInstall,
                                      time_from_device_setup,
                                      base::TimeDelta::FromMinutes(1),
                                      base::TimeDelta::FromDays(365), 50);
      }));
}

}  // namespace

void ShowCrostiniInstallerView(Profile* profile, CrostiniUISurface ui_surface) {
  base::UmaHistogramEnumeration(kCrostiniSetupSourceHistogram, ui_surface,
                                CrostiniUISurface::kCount);
  return CrostiniInstallerView::Show(profile);
}

void CrostiniInstallerView::Show(Profile* profile) {
  DCHECK(IsCrostiniUIAllowedForProfile(profile));
  if (!g_crostini_installer_view) {
    g_crostini_installer_view = new CrostiniInstallerView(profile);
    views::DialogDelegate::CreateDialogWidget(g_crostini_installer_view,
                                              nullptr, nullptr);
  }
  g_crostini_installer_view->GetDialogClientView()->SetButtonRowInsets(
      gfx::Insets(kOOBEButtonRowInset));
  g_crostini_installer_view->GetWidget()->GetRootView()->Layout();
  // We do our layout when the big message is at its biggest. Then we can
  // set it to the desired value.
  g_crostini_installer_view->SetBigMessageLabel();
  g_crostini_installer_view->GetWidget()->Show();
}

int CrostiniInstallerView::GetDialogButtons() const {
  if (state_ == State::PROMPT || state_ == State::ERROR) {
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  }
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 CrostiniInstallerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    if (state_ == State::ERROR) {
      return l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_RETRY_BUTTON);
    }
    return l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALL_BUTTON);
  }
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  if (state_ == State::INSTALL_END)
    return l10n_util::GetStringUTF16(IDS_APP_CLOSE);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}


bool CrostiniInstallerView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniInstallerView::ShouldShowWindowTitle() const {
  return false;
}

bool CrostiniInstallerView::Accept() {
  // This dialog can be accepted from State::ERROR. In that case, we're doing a
  // Retry.
  DCHECK(state_ == State::PROMPT || state_ == State::ERROR);

  state_ = State::INSTALL_START;
  profile_->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled, true);

  progress_bar_->SetVisible(true);

  // |learn_more_link_| should only be present in State::PROMPT.
  delete learn_more_link_;
  learn_more_link_ = nullptr;

  StepProgress();

  // HandleError needs the |progress_bar_|, so we delay our Offline check until
  // it exists.
  if (net::NetworkChangeNotifier::IsOffline()) {
    const base::string16 device_type = ui::GetChromeOSDeviceName();
    HandleError(l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_OFFLINE_ERROR,
                                           device_type),
                SetupResult::kErrorOffline);
    return false;  // should not close the dialog.
  }

  // Kick off the Crostini Restart sequence. We will be added as an observer.
  restart_id_ = crostini::CrostiniManager::GetInstance()->RestartCrostini(
      profile_, kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniInstallerView::StartContainerFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      this);
  return false;
}

bool CrostiniInstallerView::Cancel() {
  if (state_ != State::INSTALL_END &&
      restart_id_ != crostini::CrostiniManager::kUninitializedRestartId) {
    // Abort the long-running flow, and prevent our RestartObserver methods
    // being called after "this" has been destroyed.
    crostini::CrostiniManager::GetInstance()->AbortRestartCrostini(profile_,
                                                                   restart_id_);
    RecordSetupResultHistogram(SetupResult::kUserCancelled);
  } else {
    RecordSetupResultHistogram(SetupResult::kNotStarted);
  }
  return true;  // Should close the dialog
}

gfx::Size CrostiniInstallerView::CalculatePreferredSize() const {
  return gfx::Size(kOOBEWindowWidth, kOOBEWindowHeight);
}

void CrostiniInstallerView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(source, learn_more_link_);

  NavigateParams params(profile_, GURL(chrome::kLinuxAppsLearnMoreURL),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void CrostiniInstallerView::OnComponentLoaded(ConciergeClientResult result) {
  DCHECK_EQ(state_, State::INSTALL_START);
  state_ = State::INSTALL_IMAGE_LOADER;
  if (result != ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to install the cros-termina component";
    HandleError(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_LOAD_TERMINA_ERROR),
        SetupResult::kErrorLoadingTermina);
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
    HandleError(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_START_CONCIERGE_ERROR),
        SetupResult::kErrorStartingConcierge);
    return;
  }
  VLOG(1) << "Concierge service started";
  StepProgress();
}

void CrostiniInstallerView::OnDiskImageCreated(ConciergeClientResult result) {
  DCHECK_EQ(state_, State::START_CONCIERGE);
  state_ = State::CREATE_DISK_IMAGE;
  if (result != ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to create disk imagewith error code: "
               << static_cast<int>(result);
    HandleError(l10n_util::GetStringUTF16(
                    IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_ERROR),
                SetupResult::kErrorCreatingDiskImage);
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
                    IDS_CROSTINI_INSTALLER_START_TERMINA_VM_ERROR),
                SetupResult::kErrorStartingTermina);
    return;
  }
  VLOG(1) << "Started Termina VM successfully";
  StepProgress();
}

// static
CrostiniInstallerView* CrostiniInstallerView::GetActiveViewForTesting() {
  return g_crostini_installer_view;
}

CrostiniInstallerView::CrostiniInstallerView(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  // Layout constants from the spec.
  constexpr gfx::Insets kDialogInsets(60, 64, 32, 64);
  constexpr int kDialogSpacingVertical = 32;
  constexpr gfx::Size kLogoImageSize(32, 32);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical, gfx::Insets(), kDialogSpacingVertical));

  views::View* upper_container_view = new views::View();
  upper_container_view->SetSize(gfx::Size(
      kOOBEWindowWidth, kOOBEWindowHeight - kLinuxIllustrationHeight));
  upper_container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, kDialogInsets, kDialogSpacingVertical));
  AddChildView(upper_container_view);

  views::View* lower_container_view = new views::View();
  lower_container_view->SetSize(
      gfx::Size(kOOBEWindowWidth, kLinuxIllustrationHeight));
  views::BoxLayout* lower_container_layout =
      lower_container_view->SetLayoutManager(
          std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  AddChildView(lower_container_view);

  logo_image_ = new views::ImageView();
  logo_image_->SetImageSize(kLogoImageSize);
  logo_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_CROSTINI_DEFAULT));
  logo_image_->SetHorizontalAlignment(views::ImageView::LEADING);
  upper_container_view->AddChildView(logo_image_);

  const base::string16 device_type = ui::GetChromeOSDeviceName();

  big_message_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALLING),
      ash::AshTextContext::CONTEXT_HEADLINE_OVERSIZED);
  big_message_label_->SetMultiLine(true);
  big_message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  upper_container_view->AddChildView(big_message_label_);

  // TODO(timloh): Descenders in the message appear to be clipped, re-visit once
  // the UI has been fleshed out more.
  const base::string16 message = l10n_util::GetStringFUTF16(
      IDS_CROSTINI_INSTALLER_BODY,
      ui::FormatBytesWithUnits(kDownloadSizeInBytes, ui::DATA_UNITS_MEBIBYTE,
                               /*show_units=*/true));

  // Make a view to keep |message_label_| and |learn_more_link_| together with
  // less vertical spacing than the other dialog views.
  views::View* message_view = new views::View();
  message_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  message_label_ = new views::Label(message);
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_view->AddChildView(message_label_);

  learn_more_link_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link_->set_listener(this);
  learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_view->AddChildView(learn_more_link_);

  upper_container_view->AddChildView(message_view);

  // Make a slot for the progress bar, but it's not initially visible.
  progress_bar_ = new views::ProgressBar();
  upper_container_view->AddChildView(progress_bar_);
  progress_bar_->SetVisible(false);

  big_image_ = new views::ImageView();
  big_image_->SetImageSize(
      gfx::Size(kLinuxIllustrationWidth, kLinuxIllustrationHeight));
  big_image_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LINUX_ILLUSTRATION));
  lower_container_view->AddChildView(big_image_);

  // Make sure the lower_container_view is pinned to the bottom of the dialog.
  lower_container_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  layout->SetFlexForView(lower_container_view, 1, true);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CROSTINI_INSTALLER);
}

CrostiniInstallerView::~CrostiniInstallerView() {
  g_crostini_installer_view = nullptr;
}

void CrostiniInstallerView::HandleError(const base::string16& error_message,
                                        SetupResult result) {
  // Only ever set the error once. This check is necessary as the
  // CrostiniManager can give multiple error callbacks. Only the first should be
  // shown to the user.
  if (state_ == State::ERROR)
    return;

  RecordSetupResultHistogram(result);
  restart_id_ = crostini::CrostiniManager::kUninitializedRestartId;
  state_ = State::ERROR;
  message_label_->SetVisible(true);
  message_label_->SetText(error_message);
  SetBigMessageLabel();
  progress_bar_->SetVisible(false);

  // Remove the buttons so they get recreated with correct color and
  // highlighting. Without this it is possible for both buttons to be styled as
  // default buttons.
  delete GetDialogClientView()->ok_button();
  delete GetDialogClientView()->cancel_button();
  DialogModelChanged();
  GetWidget()->GetRootView()->Layout();
}

void CrostiniInstallerView::StartContainerFinished(
    ConciergeClientResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  state_ = State::START_CONTAINER;
  if (result != ConciergeClientResult::SUCCESS) {
    LOG(ERROR) << "Failed to start container with error code: "
               << static_cast<int>(result);
    HandleError(
        l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_START_CONTAINER_ERROR),
        SetupResult::kErrorStartingContainer);
    return;
  }
  StepProgress();
  ShowLoginShell();
}

void CrostiniInstallerView::ShowLoginShell() {
  DCHECK_EQ(state_, State::START_CONTAINER);
  state_ = State::SHOW_LOGIN_SHELL;

  crostini::CrostiniManager::GetInstance()->LaunchContainerTerminal(
      profile_, kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      std::vector<std::string>());

  StepProgress();
  RecordSetupResultHistogram(SetupResult::kSuccess);
  RecordTimeFromDeviceSetupToInstallMetric();
  GetWidget()->Close();
}

void CrostiniInstallerView::StepProgress() {
  if (State::INSTALL_START <= state_ && state_ < State::INSTALL_END) {
    // Setting value to -1 makes the progress bar play the
    // "indeterminate animation".
    progress_bar_->SetValue(-1);
    progress_bar_->SetVisible(true);
  }
  SetMessageLabel();
  SetBigMessageLabel();
  DialogModelChanged();
  GetWidget()->GetRootView()->Layout();
}

void CrostiniInstallerView::SetMessageLabel() {
  int message_id = 0;
  // The States below refer to stages that have completed.
  // The messages selected refer to the next stage, now underway.
  if (state_ == State::INSTALL_START) {
    message_id = IDS_CROSTINI_INSTALLER_LOAD_TERMINA_MESSAGE;
  } else if (state_ == State::INSTALL_IMAGE_LOADER) {
    message_id = IDS_CROSTINI_INSTALLER_START_CONCIERGE_MESSAGE;
  } else if (state_ == State::START_CONCIERGE) {
    message_id = IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_MESSAGE;
  } else if (state_ == State::CREATE_DISK_IMAGE) {
    message_id = IDS_CROSTINI_INSTALLER_START_TERMINA_VM_MESSAGE;
  } else if (state_ == State::START_TERMINA_VM) {
    message_id = IDS_CROSTINI_INSTALLER_START_CONTAINER_MESSAGE;
  }
  if (message_id != 0) {
    message_label_->SetText(l10n_util::GetStringUTF16(message_id));
    message_label_->SetVisible(true);
  } else {
    message_label_->SetVisible(false);
  }
}

void CrostiniInstallerView::SetBigMessageLabel() {
  base::string16 message;
  switch (state_) {
    case State::PROMPT: {
      const base::string16 device_type = ui::GetChromeOSDeviceName();
      message =
          l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_TITLE, device_type);
    } break;
    case State::ERROR:
      message = l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_ERROR_TITLE);
      break;
    case State::INSTALL_END:
      message = l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_COMPLETE);
      break;

    default:
      message = l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALLING);
      break;
  }
  big_message_label_->SetText(message);
  big_message_label_->SetVisible(true);
}

void CrostiniInstallerView::RecordSetupResultHistogram(SetupResult result) {
  // Prevent multiple results being logged for a given setup flow. This can
  // happen due to multiple error callbacks happening in some cases, as well
  // as the user being able to hit Cancel after any errors occur.
  if (has_logged_result_)
    return;

  base::UmaHistogramEnumeration(kCrostiniSetupResultHistogram, result,
                                SetupResult::kCount);
  has_logged_result_ = true;
}
