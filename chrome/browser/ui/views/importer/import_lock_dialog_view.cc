// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/importer/import_lock_dialog_view.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_lock_dialog.h"
#include "content/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "views/controls/label.h"

// Default size of the dialog window.
static const int kDefaultWindowWidth = 320;
static const int kDefaultWindowHeight = 100;

namespace importer {

void ShowImportLockDialog(gfx::NativeWindow parent,
                          ImporterHost* importer_host) {
  ImportLockDialogView::Show(parent, importer_host);
  UserMetrics::RecordAction(UserMetricsAction("ImportLockDialogView_Shown"));
}

}  // namespace importer

// static
void ImportLockDialogView::Show(gfx::NativeWindow parent,
                                ImporterHost* importer_host) {
  views::Widget::CreateWindow(new ImportLockDialogView(importer_host))->Show();
}

ImportLockDialogView::ImportLockDialogView(ImporterHost* importer_host)
    : description_label_(NULL),
      importer_host_(importer_host) {
  description_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_TEXT));
  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(description_label_);
}

ImportLockDialogView::~ImportLockDialogView() {
}

gfx::Size ImportLockDialogView::GetPreferredSize() {
  return gfx::Size(views::Widget::GetLocalizedContentsSize(
      IDS_IMPORTLOCK_DIALOG_WIDTH_CHARS,
      IDS_IMPORTLOCK_DIALOG_HEIGHT_LINES));
}

void ImportLockDialogView::Layout() {
  description_label_->SetBounds(
      views::kPanelHorizMargin,
      views::kPanelVertMargin,
      width() - 2 * views::kPanelHorizMargin,
      height() - 2 * views::kPanelVertMargin);
}

string16 ImportLockDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_OK);
  else if (button == ui::DIALOG_BUTTON_CANCEL)
    return l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_CANCEL);
  return string16();
}

bool ImportLockDialogView::IsModal() const {
  return false;
}

string16 ImportLockDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_TITLE);
}

bool ImportLockDialogView::Accept() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ImporterHost::OnImportLockDialogEnd,
                 importer_host_.get(), true));
  return true;
}

bool ImportLockDialogView::Cancel() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ImporterHost::OnImportLockDialogEnd,
                 importer_host_.get(), false));
  return true;
}

views::View* ImportLockDialogView::GetContentsView() {
  return this;
}
