// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_installer_view.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"
#include "chrome/browser/ui/app_list/crostini/crostini_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
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

void CrostiniInstallerView::Show(const CrostiniAppItem* app_item) {
  DCHECK(IsExperimentalCrostiniUIAvailable());
  if (!g_crostini_installer_view) {
    g_crostini_installer_view = new CrostiniInstallerView(app_item);
    views::DialogDelegate::CreateDialogWidget(g_crostini_installer_view,
                                              nullptr, nullptr);
  }
  g_crostini_installer_view->GetWidget()->Show();
}

int CrostiniInstallerView::GetDialogButtons() const {
  return state_ == State::PROMPT
             ? ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL
             : ui::DIALOG_BUTTON_CANCEL;
}

base::string16 CrostiniInstallerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_INSTALL_BUTTON);
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

base::string16 CrostiniInstallerView::GetWindowTitle() const {
  if (state_ == State::PROMPT) {
    const base::string16 device_type = ui::GetChromeOSDeviceName();
    return l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_TITLE, app_name_,
                                      device_type);
  }
  return l10n_util::GetStringFUTF16(IDS_CROSTINI_INSTALLER_INSTALLING,
                                    app_name_);
}

bool CrostiniInstallerView::ShouldShowCloseButton() const {
  return false;
}

bool CrostiniInstallerView::Accept() {
  DCHECK_EQ(state_, State::PROMPT);
  state_ = State::INSTALL;
  DialogModelChanged();
  GetWidget()->UpdateWindowTitle();

  message_label_->SetVisible(false);

  progress_bar_ = new views::ProgressBar();
  // Values outside the range [0,1] display an infinite loading animation.
  progress_bar_->SetValue(-1);
  AddChildView(progress_bar_);

  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());

  // TODO(813699): Begin the actual crostini install flow.
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

CrostiniInstallerView::CrostiniInstallerView(const CrostiniAppItem* app_item)
    : app_name_(base::ASCIIToUTF16(app_item->name())) {
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
