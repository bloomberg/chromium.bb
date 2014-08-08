// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"
#include "ui/views/border.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

// static
void DownloadInProgressDialogView::Show(
    gfx::NativeWindow parent,
    int download_count,
    Browser::DownloadClosePreventionType dialog_type,
    bool app_modal,
    const base::Callback<void(bool)>& callback) {
  DownloadInProgressDialogView* window = new DownloadInProgressDialogView(
      download_count, dialog_type, app_modal, callback);
  CreateBrowserModalDialogViews(window, parent)->Show();
}

DownloadInProgressDialogView::DownloadInProgressDialogView(
    int download_count,
    Browser::DownloadClosePreventionType dialog_type,
    bool app_modal,
    const base::Callback<void(bool)>& callback)
    : app_modal_(app_modal),
      callback_(callback),
      message_box_view_(NULL) {
  base::string16 explanation_text;
  switch (dialog_type) {
    case Browser::DOWNLOAD_CLOSE_BROWSER_SHUTDOWN:
      if (download_count == 1) {
        title_text_ = l10n_util::GetStringUTF16(
            IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_TITLE);
        explanation_text = l10n_util::GetStringUTF16(
            IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_EXPLANATION);
      } else {
        title_text_ = l10n_util::GetStringUTF16(
            IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_TITLE);
        explanation_text = l10n_util::GetStringUTF16(
            IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_EXPLANATION);
      }
      ok_button_text_ = l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_REMOVE_CONFIRM_OK_BUTTON_LABEL);
      break;
    case Browser::DOWNLOAD_CLOSE_LAST_WINDOW_IN_INCOGNITO_PROFILE:
      if (download_count == 1) {
        title_text_ = l10n_util::GetStringUTF16(
            IDS_SINGLE_INCOGNITO_DOWNLOAD_REMOVE_CONFIRM_TITLE);
        explanation_text = l10n_util::GetStringUTF16(
            IDS_SINGLE_INCOGNITO_DOWNLOAD_REMOVE_CONFIRM_EXPLANATION);
      } else {
        title_text_ = l10n_util::GetStringUTF16(
            IDS_MULTIPLE_INCOGNITO_DOWNLOADS_REMOVE_CONFIRM_TITLE);
        explanation_text = l10n_util::GetStringUTF16(
            IDS_MULTIPLE_INCOGNITO_DOWNLOADS_REMOVE_CONFIRM_EXPLANATION);
      }
      ok_button_text_ = l10n_util::GetStringUTF16(
          IDS_INCOGNITO_DOWNLOAD_REMOVE_CONFIRM_OK_BUTTON_LABEL);
      break;
    default:
      // This dialog should have been created within the same thread invocation
      // as the original test that lead to us, so it should always not be ok
      // to close.
      NOTREACHED();
  }
  cancel_button_text_ = l10n_util::GetStringUTF16(
      IDS_DOWNLOAD_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL);

  message_box_view_ = new views::MessageBoxView(
      views::MessageBoxView::InitParams(explanation_text));
}

DownloadInProgressDialogView::~DownloadInProgressDialogView() {}

int DownloadInProgressDialogView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 DownloadInProgressDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return (button == ui::DIALOG_BUTTON_OK) ?
      ok_button_text_ : cancel_button_text_;
}

bool DownloadInProgressDialogView::Cancel() {
  callback_.Run(false);
  return true;
}

bool DownloadInProgressDialogView::Accept() {
  callback_.Run(true);
  return true;
}

ui::ModalType DownloadInProgressDialogView::GetModalType() const {
  return app_modal_ ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_WINDOW;
}

base::string16 DownloadInProgressDialogView::GetWindowTitle() const {
  return title_text_;
}

void DownloadInProgressDialogView::DeleteDelegate() {
  delete this;
}

views::Widget* DownloadInProgressDialogView::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* DownloadInProgressDialogView::GetWidget() const {
  return message_box_view_->GetWidget();
}

views::View* DownloadInProgressDialogView::GetContentsView() {
  return message_box_view_;
}
