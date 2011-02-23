// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/importer_lock_view.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/importer/importer.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/label.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

// Default size of the dialog window.
static const int kDefaultWindowWidth = 320;
static const int kDefaultWindowHeight = 100;

namespace browser {

void ShowImportLockDialog(gfx::NativeWindow parent,
                          ImporterHost* importer_host) {
  ImporterLockView::Show(parent, importer_host);
}

}  // namespace browser

// static
void ImporterLockView::Show(gfx::NativeWindow parent,
                            ImporterHost* importer_host) {
  views::Window::CreateChromeWindow(
      NULL, gfx::Rect(),
      new ImporterLockView(importer_host))->Show();
}

ImporterLockView::ImporterLockView(ImporterHost* host)
    : description_label_(NULL),
      importer_host_(host) {
  description_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_TEXT)));
  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(description_label_);
}

ImporterLockView::~ImporterLockView() {
}

gfx::Size ImporterLockView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_IMPORTLOCK_DIALOG_WIDTH_CHARS,
      IDS_IMPORTLOCK_DIALOG_HEIGHT_LINES));
}

void ImporterLockView::Layout() {
  description_label_->SetBounds(
      views::kPanelHorizMargin,
      views::kPanelVertMargin,
      width() - 2 * views::kPanelHorizMargin,
      height() - 2 * views::kPanelVertMargin);
}

std::wstring ImporterLockView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_OK));
  } else if (button == MessageBoxFlags::DIALOGBUTTON_CANCEL) {
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_CANCEL));
  }
  return std::wstring();
}

bool ImporterLockView::IsModal() const {
  return false;
}

std::wstring ImporterLockView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORTER_LOCK_TITLE));
}

bool ImporterLockView::Accept() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      importer_host_, &ImporterHost::OnLockViewEnd, true));
  return true;
}

bool ImporterLockView::Cancel() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      importer_host_, &ImporterHost::OnLockViewEnd, false));
  return true;
}

views::View* ImporterLockView::GetContentsView() {
  return this;
}
