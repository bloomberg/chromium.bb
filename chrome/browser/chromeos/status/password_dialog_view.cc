// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/password_dialog_view.h"

#include "app/l10n_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/textfield/textfield.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

PasswordDialogView::PasswordDialogView(PasswordDialogDelegate* delegate,
                                       const std::string& ssid)
    : delegate_(delegate),
      ssid_(ssid),
      password_textfield_(NULL) {
  Init();
}

bool PasswordDialogView::Cancel() {
  return delegate_->OnPasswordDialogCancel();
}

bool PasswordDialogView::Accept() {
  // TODO(chocobo): We should not need to call SyncText ourself here.
  password_textfield_->SyncText();
  return delegate_->OnPasswordDialogAccept(ssid_, password_textfield_->text());
}

std::wstring PasswordDialogView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_PASSWORD);
}

gfx::Size PasswordDialogView::GetPreferredSize() {
  // TODO(chocobo): Create our own localized content size once the UI is done.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_IMPORTLOCK_DIALOG_WIDTH_CHARS,
      IDS_IMPORTLOCK_DIALOG_HEIGHT_LINES));
}

void PasswordDialogView::Init() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 200);

  layout->StartRow(0, column_view_set_id);
  password_textfield_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);
  layout->AddView(password_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace chromeos
