// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_html_dialog.h"

#include "chrome/browser/profile_manager.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "gfx/native_widget_types.h"
#include "gfx/size.h"
#include "views/window/window.h"

namespace chromeos {

namespace {

const int kDefaultWidth = 800;
const int kDefaultHeight = 600;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LoginHtmlDialog, public:

LoginHtmlDialog::LoginHtmlDialog(Delegate* delegate,
                                 gfx::NativeWindow parent_window,
                                 const std::wstring& title,
                                 const GURL& url)
    : delegate_(delegate),
      parent_window_(parent_window),
      title_(title),
      url_(url) {
}

LoginHtmlDialog::~LoginHtmlDialog() {
  delegate_ = NULL;
}

void LoginHtmlDialog::Show() {
  browser::ShowHtmlDialogView(parent_window_,
                              ProfileManager::GetDefaultProfile(),
                              this);
}

///////////////////////////////////////////////////////////////////////////////
// LoginHtmlDialog, protected:

void LoginHtmlDialog::OnDialogClosed(const std::string& json_retval) {
  if (delegate_)
    delegate_->OnDialogClosed();
}

void LoginHtmlDialog::OnCloseContents(TabContents* source,
                                      bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

void LoginHtmlDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

}  // namespace chromeos
