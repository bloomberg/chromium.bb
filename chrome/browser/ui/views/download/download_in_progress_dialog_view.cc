// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"
#include "views/border.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/window/window.h"

DownloadInProgressDialogView::DownloadInProgressDialogView(Browser* browser)
    : browser_(browser),
      product_name_(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)) {
  int download_count = browser->profile()->GetDownloadManager()->
      in_progress_count();

  std::wstring warning_text;
  std::wstring explanation_text;
  if (download_count == 1) {
    warning_text = UTF16ToWide(l10n_util::GetStringFUTF16(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_WARNING,
        product_name_));
    explanation_text = UTF16ToWide(l10n_util::GetStringFUTF16(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_EXPLANATION,
        product_name_));
    ok_button_text_ = UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_OK_BUTTON_LABEL));
    cancel_button_text_ = UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL));
  } else {
    warning_text = UTF16ToWide(l10n_util::GetStringFUTF16(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_WARNING,
        product_name_,
        UTF8ToUTF16(base::IntToString(download_count))));
    explanation_text = UTF16ToWide(l10n_util::GetStringFUTF16(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_EXPLANATION,
        product_name_));
    ok_button_text_ = UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_OK_BUTTON_LABEL));
    cancel_button_text_ = UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL));
  }

  // There are two lines of text: the bold warning label and the text
  // explanation label.
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int columnset_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(columnset_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  gfx::Font bold_font =
      ResourceBundle::GetSharedInstance().GetFont(
          ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  warning_ = new views::Label(warning_text, bold_font);
  warning_->SetMultiLine(true);
  warning_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  warning_->set_border(views::Border::CreateEmptyBorder(10, 10, 10, 10));
  layout->StartRow(0, columnset_id);
  layout->AddView(warning_);

  explanation_ = new views::Label(explanation_text);
  explanation_->SetMultiLine(true);
  explanation_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  explanation_->set_border(views::Border::CreateEmptyBorder(10, 10, 10, 10));
  layout->StartRow(0, columnset_id);
  layout->AddView(explanation_);

  dialog_dimensions_ = views::Window::GetLocalizedContentsSize(
      IDS_DOWNLOAD_IN_PROGRESS_WIDTH_CHARS,
      IDS_DOWNLOAD_IN_PROGRESS_MINIMUM_HEIGHT_LINES);
  const int height =
      warning_->GetHeightForWidth(dialog_dimensions_.width()) +
      explanation_->GetHeightForWidth(dialog_dimensions_.width());
  dialog_dimensions_.set_height(std::max(height,
                                         dialog_dimensions_.height()));
}

DownloadInProgressDialogView::~DownloadInProgressDialogView() {}

gfx::Size DownloadInProgressDialogView::GetPreferredSize() {
  return dialog_dimensions_;
}

std::wstring DownloadInProgressDialogView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return ok_button_text_;

  DCHECK_EQ(MessageBoxFlags::DIALOGBUTTON_CANCEL, button);
  return cancel_button_text_;
}

int DownloadInProgressDialogView::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

bool DownloadInProgressDialogView::Cancel() {
  browser_->InProgressDownloadResponse(false);
  return true;
}

bool DownloadInProgressDialogView::Accept() {
  browser_->InProgressDownloadResponse(true);
  return true;
}

bool DownloadInProgressDialogView::IsModal() const {
  return true;
}

std::wstring DownloadInProgressDialogView::GetWindowTitle() const {
  return UTF16ToWideHack(product_name_);
}

views::View* DownloadInProgressDialogView::GetContentsView() {
  return this;
}
