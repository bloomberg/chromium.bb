// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/missing_system_file_dialog_win.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

// static
bool MissingSystemFileDialog::dialog_shown_ = false;

// static
void MissingSystemFileDialog::ShowDialog(gfx::NativeWindow parent,
                                         Profile* profile) {
  if (dialog_shown_)
    return;
  dialog_shown_ = true;
  MissingSystemFileDialog* dialog = new MissingSystemFileDialog(profile);
  views::Widget::CreateWindowWithParent(dialog, parent)->Show();
}

MissingSystemFileDialog::MissingSystemFileDialog(Profile* profile)
    : profile_(profile) {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int column_set_id = 0;
  views::ColumnSet* columns = layout->AddColumnSet(column_set_id);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                     0, views::GridLayout::USE_PREF, 0, 0);

  views::Label* message = new views::Label(
      l10n_util::GetStringUTF16(IDS_MISSING_WINDOWS_SYSTEM_FILES_MESSAGE));
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  const int kMessageWidth = 400;
  message->SizeToFit(kMessageWidth);

  views::Link* link = new views::Link(
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  link->set_listener(this);
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  layout->StartRow(0, column_set_id);
  layout->AddView(message);

  layout->StartRow(0, column_set_id);
  layout->AddView(link);
}

MissingSystemFileDialog::~MissingSystemFileDialog() {}

views::View* MissingSystemFileDialog::GetContentsView() {
  return this;
}

string16 MissingSystemFileDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_MISSING_WINDOWS_SYSTEM_FILES_TITLE);
}

ui::ModalType MissingSystemFileDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

int MissingSystemFileDialog::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

void MissingSystemFileDialog::LinkClicked(views::Link* source,
                                          int event_flags) {
  const char kBlankOmniboxHelpUrl[] =
      "https://support.google.com/chrome/?p=e_blankomnibox";

  chrome::NavigateParams params(profile_,
                                GURL(kBlankOmniboxHelpUrl),
                                content::PAGE_TRANSITION_LINK);
  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  params.disposition =
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition;
  chrome::Navigate(&params);

  GetWidget()->Close();
}
