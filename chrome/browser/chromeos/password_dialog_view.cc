// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/password_dialog_view.h"

#include "app/l10n_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/textfield/textfield.h"
#include "views/window/window.h"

namespace chromeos {

PasswordDialogView::PasswordDialogView(PasswordDialogDelegate* delegate,
                                       const std::string& ssid)
    : delegate_(delegate),
      ssid_(ssid),
      password_textfield_(NULL) {
}

std::wstring PasswordDialogView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_PASSWORD);
}

bool PasswordDialogView::Cancel() {
  return delegate_->OnPasswordDialogCancel();
}

bool PasswordDialogView::Accept() {
  // TODO(chocobo): We should not need to call SyncText ourself here.
  password_textfield_->SyncText();
  return delegate_->OnPasswordDialogAccept(ssid_, password_textfield_->text());
}

static const int kDialogPadding = 7;

void PasswordDialogView::Layout() {
  gfx::Size sz = password_textfield_->GetPreferredSize();
  password_textfield_->SetBounds(kDialogPadding, kDialogPadding,
                                 width() - 2*kDialogPadding,
                                 sz.height());
}

gfx::Size PasswordDialogView::GetPreferredSize() {
  // TODO(chocobo): Create our own localized content size once the UI is done.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_IMPORTLOCK_DIALOG_WIDTH_CHARS,
      IDS_IMPORTLOCK_DIALOG_HEIGHT_LINES));
}

void PasswordDialogView::ViewHierarchyChanged(bool is_add,
                                              views::View* parent,
                                              views::View* child) {
  if (is_add && child == this)
    Init();
}

void PasswordDialogView::Init() {
  password_textfield_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);
  AddChildView(password_textfield_);
}

}  // namespace chromeos
