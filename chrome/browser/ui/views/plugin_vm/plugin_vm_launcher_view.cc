// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plugin_vm/plugin_vm_launcher_view.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
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

namespace {

PluginVmLauncherView* g_plugin_vm_launcher_view = nullptr;

}  // namespace

void plugin_vm::ShowPluginVmLauncherView(Profile* profile) {
  if (!g_plugin_vm_launcher_view) {
    g_plugin_vm_launcher_view = new PluginVmLauncherView(profile);
    views::DialogDelegate::CreateDialogWidget(g_plugin_vm_launcher_view,
                                              nullptr, nullptr);
  }
  g_plugin_vm_launcher_view->GetWidget()->Show();
}

PluginVmLauncherView::PluginVmLauncherView(Profile* profile)
    : plugin_vm_image_manager_(
          plugin_vm::PluginVmImageManagerFactory::GetForProfile(profile)) {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG)));

  message_label_ = new views::Label();
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label_);

  progress_bar_ = new views::ProgressBar();
  progress_bar_->SetVisible(false);
  AddChildView(progress_bar_);
}

int PluginVmLauncherView::GetDialogButtons() const {
  switch (state_) {
    case State::START_DOWNLOADING:
    case State::DOWNLOADING:
    case State::UNZIPPING:
      return ui::DIALOG_BUTTON_CANCEL;
    case State::FINISHED:
      return ui::DIALOG_BUTTON_OK;
    case State::ERROR:
      return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
  }
}

base::string16 PluginVmLauncherView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (state_) {
    case State::START_DOWNLOADING:
    case State::DOWNLOADING:
    case State::UNZIPPING: {
      DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
      return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
    }
    case State::FINISHED: {
      DCHECK_EQ(button, ui::DIALOG_BUTTON_OK);
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_LAUNCH_BUTTON);
    }
    case State::ERROR: {
      return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                           ? IDS_PLUGIN_VM_LAUNCHER_RETRY_BUTTON
                                           : IDS_APP_CANCEL);
    }
  }
}

base::string16 PluginVmLauncherView::GetWindowTitle() const {
  switch (state_) {
    case State::START_DOWNLOADING:
    case State::DOWNLOADING:
    case State::UNZIPPING:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_LAUNCHER_ENVIRONMENT_SETTING_TITLE);
    case State::FINISHED:
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_FINISHED_TITLE);
    case State::ERROR:
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_ERROR_TITLE);
  }
}

bool PluginVmLauncherView::Accept() {
  if (state_ == State::FINISHED) {
    // Launch button has been clicked.
    // TODO(https://crbug.com/904852): Launch PluginVm.
    return true;
  }
  DCHECK_EQ(state_, State::ERROR);
  // Retry button has been clicked to retry setting of PluginVm environment
  // after error occurred.
  StartPluginVmImageDownload();
  return false;
}

bool PluginVmLauncherView::Cancel() {
  if (state_ == State::DOWNLOADING)
    plugin_vm_image_manager_->CancelDownload();
  if (state_ == State::UNZIPPING)
    plugin_vm_image_manager_->CancelUnzipping();
  return true;
}

gfx::Size PluginVmLauncherView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}

PluginVmLauncherView::~PluginVmLauncherView() {
  plugin_vm_image_manager_->RemoveObserver();
  g_plugin_vm_launcher_view = nullptr;
}

void PluginVmLauncherView::OnDownloadStarted() {}

void PluginVmLauncherView::OnProgressUpdated(
    base::Optional<double> fraction_complete) {
  DCHECK_EQ(state_, State::DOWNLOADING);
  if (fraction_complete.has_value())
    progress_bar_->SetValue(fraction_complete.value());
  else
    progress_bar_->SetValue(-1);
}

void PluginVmLauncherView::OnDownloadCompleted() {
  DCHECK_EQ(state_, State::DOWNLOADING);

  plugin_vm_image_manager_->StartUnzipping();
  state_ = State::UNZIPPING;
  OnStateUpdated();
}

void PluginVmLauncherView::OnDownloadCancelled() {}

void PluginVmLauncherView::OnDownloadFailed() {
  state_ = State::ERROR;
  OnStateUpdated();
}

void PluginVmLauncherView::OnUnzipped() {
  DCHECK_EQ(state_, State::UNZIPPING);
  state_ = State::FINISHED;
  OnStateUpdated();
}

void PluginVmLauncherView::OnUnzippingFailed() {
  state_ = State::ERROR;
  OnStateUpdated();
}

plugin_vm::PluginVmImageManager*
PluginVmLauncherView::GetPluginVmImageManagerForTesting() {
  return plugin_vm_image_manager_;
}

void PluginVmLauncherView::AddedToWidget() {
  StartPluginVmImageDownload();
}

base::string16 PluginVmLauncherView::GetMessage() const {
  switch (state_) {
    case State::START_DOWNLOADING:
    case State::FINISHED:
      return base::string16();
    case State::DOWNLOADING:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_LAUNCHER_DOWNLOADING_MESSAGE);
    case State::UNZIPPING:
      return l10n_util::GetStringUTF16(
          IDS_PLUGIN_VM_LAUNCHER_UNZIPPING_MESSAGE);
    case State::ERROR:
      return l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_ERROR_MESSAGE);
  }
}

void PluginVmLauncherView::OnStateUpdated() {
  DialogModelChanged();
  GetWidget()->UpdateWindowTitle();

  base::string16 message = GetMessage();
  const bool message_visible = !message.empty();
  message_label_->SetVisible(message_visible);
  message_label_->SetText(message);

  const bool progress_bar_visible =
      state_ == State::DOWNLOADING || state_ == State::UNZIPPING;
  progress_bar_->SetVisible(progress_bar_visible);
  // Values outside the range [0,1] display an infinite loading animation.
  progress_bar_->SetValue(-1);

  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

void PluginVmLauncherView::StartPluginVmImageDownload() {
  plugin_vm_image_manager_->SetObserver(this);
  plugin_vm_image_manager_->StartDownload();

  state_ = State::DOWNLOADING;
  OnStateUpdated();
}
